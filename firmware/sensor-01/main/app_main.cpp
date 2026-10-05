/* homecadia sensor-01 — Matter-over-Thread temperature/humidity sensor.
 *
 * Endpoints: temperature sensor, humidity sensor, power source (battery).
 * SHT40 polled every SENSOR_POLL_INTERVAL_S; attributes update on delta
 * (sensor_loop.cpp). Structure follows the esp-matter icd_app example (v1.6).
 */

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#include "sleep_diag.h"
#endif
#include <esp_attr.h>
#include <esp_rom_sys.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_matter.h>
#include <esp_matter_ota.h>

#include <common_macros.h>
#include <app_config.h>
#include <bench_selftest.h>
#include <display.h>
#include <led.h>
#include <sensor_loop.h>
#include <settings.h>
#include <ui.h>

#include <setup_payload/OnboardingCodesUtil.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <esp_openthread_types.h>
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>

static const char *TAG = "sensor-01";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;

constexpr auto k_commissioning_window_timeout = chip::System::Clock::Seconds16(300);
constexpr int k_decommission_reboot_delay_s = 3;
#if CONFIG_HOMECADIA_UNPAIRED_DEEP_SLEEP
constexpr auto k_unpaired_check_delay = chip::System::Clock::Seconds16(30);
/* Past this much uptime an unpaired unit sleeps even if the window still
 * reads as open: after a failed pairing the SDK re-opens the window for a
 * retry with no timeout of its own (CommissioningWindowManager::
 * HandleFailedAttempt), and on 2026-10-02 that kept a unit awake at 67 mA
 * past the 15-minute window. One minute over the SDK's 15-minute window. */
constexpr int64_t k_unpaired_awake_cap_us = (int64_t)(CHIP_DEVICE_CONFIG_DISCOVERY_TIMEOUT_SECS + 60) * 1000000;
constexpr uint32_t k_asleep_screen_timeout_ms = 8000; /* full refresh is ~1.8 s */
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG() \
    {                                         \
        .radio_mode = RADIO_MODE_NATIVE,      \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()               \
    {                                                      \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE, \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, \
    }
#endif

/* Renders the commissioning QR + manual code on the e-ink (they also go to
 * the serial console via the stack's own PrintOnboardingCodes). With asleep
 * set, adds the "turn the dial" line and returns once it is on the panel. */
static void show_commissioning_screen(bool asleep = false)
{
    char qr[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1];
    char manual[chip::kManualSetupLongCodeCharLength + 1];
    chip::MutableCharSpan qr_span(qr);
    chip::MutableCharSpan manual_span(manual);
    if (GetQRCode(qr_span, chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE)) != CHIP_NO_ERROR ||
        GetManualPairingCode(manual_span, chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE)) !=
            CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Failed to get onboarding codes");
        return;
    }
    qr[qr_span.size()] = '\0';
    manual[manual_span.size()] = '\0';
#if CONFIG_HOMECADIA_UNPAIRED_DEEP_SLEEP
    if (asleep) {
        esp_err_t err = display_show_pairing_asleep(qr, manual, k_asleep_screen_timeout_ms);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Asleep screen not confirmed: %s", esp_err_to_name(err));
        }
        return;
    }
#else
    (void)asleep;
#endif
    display_show_commissioning(qr, manual);
}

#if CONFIG_HOMECADIA_UNPAIRED_DEEP_SLEEP
/* An unpaired unit never light-sleeps: ESP-IDF's OpenThread port holds its
 * "ot_sleep" PM lock until the 802.15.4 radio reports the sleep state
 * (esp_openthread_sleep.c), which needs a Thread network. Measured 28 mA
 * (field-notes.md section 26). So once the commissioning window has closed
 * with no fabric, the unit deep-sleeps and a turn of the dial (encoder A, an
 * LP GPIO) brings it back with a new window.
 *
 * Restart first, sleep second. The running system draws the "asleep" screen
 * and restarts with a request flag set; the next boot enters deep sleep from
 * the top of app_main(), before Bluetooth, Thread, the PHY or power
 * management exist. Entering deep sleep from the running system worked on USB
 * and stuck at ~20 mA on battery (bringup.md, 2026-10-01), and the IDF docs
 * require the radios to be stopped before deep sleep (sleep_modes.rst). */
#define WAKE_INFO_MAGIC 0x57414B45u  /* "WAKE": the RTC block below holds data, not power-on noise */
#define SLEEP_REQUEST_MAGIC 0x534C5050u /* "SLPP": the restart was asked for by unpaired_sleep_check() */
static RTC_NOINIT_ATTR uint32_t s_wake_magic;
static RTC_NOINIT_ATTR uint32_t s_wake_count;
static RTC_NOINIT_ATTR uint32_t s_wake_cause;
static RTC_NOINIT_ATTR uint32_t s_sleep_request;

/* The wake pin keeps a pull-up through deep sleep and wakes on the level it
 * is not at, as in ec11.c arm_wake(). CONFIG_PM_SLP_DISABLE_GPIO has already
 * set every pin to switch to an isolated, floating state in sleep
 * (esp_sleep_startup_init, sleep_gpio.c); left on, the wake pin floated and
 * the unit woke by itself within seconds. Never returns. */
static void enter_unpaired_deep_sleep(void)
{
#if CONFIG_PM_SLP_DISABLE_GPIO
    gpio_sleep_sel_dis((gpio_num_t)ENC_PIN_A);
#endif
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << ENC_PIN_A;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);
    esp_rom_delay_us(100); /* let the pull-up settle before reading the rest level */
    esp_deepsleep_gpio_wake_up_mode_t mode =
        gpio_get_level((gpio_num_t)ENC_PIN_A) ? ESP_GPIO_WAKEUP_GPIO_LOW : ESP_GPIO_WAKEUP_GPIO_HIGH;
    esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(BIT64(ENC_PIN_A), mode);
    if (err != ESP_OK) {
        /* Without a wake source deep sleep would need a power cycle to leave:
         * boot normally instead and stay awake. */
        ESP_LOGE(TAG, "Deep-sleep wake on GPIO%d failed (%s): staying awake", ENC_PIN_A, esp_err_to_name(err));
        return;
    }
    esp_deep_sleep_start();
}

/* First thing in app_main(). */
static void unpaired_sleep_on_boot(void)
{
    if (s_wake_magic != WAKE_INFO_MAGIC) {
        s_wake_magic = WAKE_INFO_MAGIC;
        s_wake_count = 0;
        s_wake_cause = 0;
        s_sleep_request = 0;
    }
    esp_reset_reason_t reason = esp_reset_reason();
    if (reason == ESP_RST_DEEPSLEEP) {
        /* Woken by the dial. This boot inherits the wake pin's pad hold and
         * LP wake setting, and on 2026-09-30 such a boot hung in the GPIO
         * interrupt setup at ~20 mA. Undo both and restart cleanly. */
        s_wake_count++;
        s_wake_cause = (uint32_t)esp_sleep_get_wakeup_cause();
        gpio_hold_dis((gpio_num_t)ENC_PIN_A);
        gpio_deep_sleep_wakeup_disable((gpio_num_t)ENC_PIN_A);
        esp_restart();
    }
    bool requested = s_sleep_request == SLEEP_REQUEST_MAGIC;
    s_sleep_request = 0; /* one shot: whatever happens next, the following boot is a normal one */
    if (requested && reason == ESP_RST_SW) {
        ESP_LOGW(TAG, "Unpaired: entering deep sleep from early boot; turn the dial to wake");
        enter_unpaired_deep_sleep();
    }
}

static void unpaired_sleep_check(chip::System::Layer *layer, void *);

static void schedule_unpaired_sleep_check(void)
{
    chip::DeviceLayer::SystemLayer().CancelTimer(unpaired_sleep_check, nullptr); /* one pending check at a time */
    CHIP_ERROR err =
        chip::DeviceLayer::SystemLayer().StartTimer(k_unpaired_check_delay, unpaired_sleep_check, nullptr);
    if (err != CHIP_NO_ERROR) {
        ESP_LOGE(TAG, "Unpaired sleep check not scheduled: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

static void unpaired_sleep_check(chip::System::Layer *, void *forced)
{
    chip::Server &server = chip::Server::GetInstance();
    if (server.GetFabricTable().FabricCount() != 0) {
        return;
    }
    /* A commissioner mid-way holds the fail-safe; never cut that off (the SDK
     * caps a fail-safe at 15 min). An open window holds the unit only until
     * the uptime cap. */
    bool pairing_in_progress = server.GetFailSafeContext().IsFailSafeArmed();
    bool window_open = server.GetCommissioningWindowManager().IsCommissioningWindowOpen();
    bool over_cap = esp_timer_get_time() > k_unpaired_awake_cap_us;
    if (!forced && (pairing_in_progress || (window_open && !over_cap))) {
        schedule_unpaired_sleep_check();
        return;
    }
    ESP_LOGW(TAG, "Unpaired and the commissioning window is closed: restarting into deep sleep");
    show_commissioning_screen(true); /* the panel keeps this image through the sleep */
    s_sleep_request = SLEEP_REQUEST_MAGIC;
    esp_restart();
}
#else
static void schedule_unpaired_sleep_check(void) {} /* feature off (bench profile: the console needs USB) */
static void unpaired_sleep_on_boot(void) {}
#endif

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete: {
        ESP_LOGI(TAG, "Commissioning complete");
        led_set_commissioning(false);
        display_commissioning_done(); /* release the onboarding-screen latch */
        sensor_readings_t r = sensor_loop_get_readings();
        if (r.valid) {
            display_show_readings(&r);
        }
        break;
    }

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        led_set_commissioning(true);
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        led_set_commissioning(false);
        schedule_unpaired_sleep_check();
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        schedule_unpaired_sleep_check();
        break;

    case chip::DeviceLayer::DeviceEventType::kThreadConnectivityChange:
        ESP_LOGI(TAG, "Thread connectivity change: %d", event->ThreadConnectivityChange.Result);
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved: {
        ESP_LOGI(TAG, "Fabric removed");
        /* Last controller un-paired us: reopen the commissioning window so the
         * device can be re-adopted without a manual factory reset.
         *
         * kAllSupported, not kDnssdOnly: removing the last fabric also takes us
         * off Thread, so a DNS-SD-only window advertises on a network we are no
         * longer attached to -- i.e. nowhere. Verified on hardware 2026-08-24:
         * after remove_node the device went silent on BLE and could only be
         * recovered by power-cycling it, which is exactly the manual
         * intervention this block exists to avoid. */
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
            /* Put the codes back on the panel before we go down. */
            show_commissioning_screen();

            /* Reboot rather than just reopening the window here. With
             * CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING the stack tears BLE down
             * once commissioning finishes and hands its RAM back to the heap
             * (BLEManagerImpl::DeinitESPBleLayer -> ClaimBLEMemory, ESP32
             * nimble/BLEManagerImpl.cpp), which cannot be undone in place. So a
             * window opened now advertises over DNS-SD on a network we just
             * left, and over a BLE stack that no longer exists -- verified on
             * hardware 2026-08-24: removeFabric returned 0 and the device then
             * advertised nothing at all.
             *
             * A fresh boot with no fabrics brings BLE up and advertises the way
             * first boot does. The delay lets the RemoveFabric response and the
             * Leave event reach the controller first. */
            ESP_LOGW(TAG, "Last fabric removed: rebooting in %ds to re-advertise for commissioning",
                     k_decommission_reboot_delay_s);
            esp_timer_handle_t t = nullptr;
            const esp_timer_create_args_t args = {
                .callback = [](void *) { esp_restart(); },
                .name = "decommission_reboot",
            };
            if (esp_timer_create(&args, &t) == ESP_OK) {
                esp_timer_start_once(t, (uint64_t)k_decommission_reboot_delay_s * 1000000ULL);
            } else {
                ESP_LOGE(TAG, "Reboot timer failed; restarting immediately");
                esp_restart();
            }
        }
        break;
    }

    default:
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    /* Identify cluster: milestone 3+ blinks the display or LED here. */
    ESP_LOGI(TAG, "Identify: type %u, effect %u, variant %u", type, effect_id, effect_variant);
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    /* No writable application attributes yet. */
    return ESP_OK;
}

/* XIAO ESP32-C6 RF path (schematic sheet 4/5): the FM8625H antenna switch is
 * unpowered at reset (Q3 gate pulled high). Power it and select the ceramic
 * antenna before any radio starts, or BLE commissioning and Thread run with
 * no antenna. */
static void board_rf_switch_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << RF_SWITCH_POWER_GPIO) | (1ULL << RF_ANT_SELECT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level((gpio_num_t)RF_SWITCH_POWER_GPIO, 0); /* active low: switch on */
    gpio_set_level((gpio_num_t)RF_ANT_SELECT_GPIO, 0);   /* ceramic antenna */
}

extern "C" void app_main()
{
#if CONFIG_HOMECADIA_UNPAIRED_SLEEP_TEST_S > 0
    vTaskDelay(pdMS_TO_TICKS(3000)); /* let a re-enumerating USB console catch the boot */
#endif
    unpaired_sleep_on_boot();
#if CONFIG_HOMECADIA_UNPAIRED_SLEEP_TEST_S > 0
    {
        char line[26];
        snprintf(line, sizeof(line), "rst%d wakes%lu cause%lu", (int)esp_reset_reason(), (unsigned long)s_wake_count,
                 (unsigned long)s_wake_cause);
        display_set_debug_line(line);
    }
#endif
    ESP_LOGW(TAG, "boot: reset reason %d, wakeup cause %d", (int)esp_reset_reason(), (int)esp_sleep_get_wakeup_cause());

    /* Before anything claims a pin: a pin a driver owns reports the driver. */
    bench_selftest();
    board_rf_switch_init();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

#if CONFIG_PM_ENABLE
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true,
#endif
    };
    err = esp_pm_configure(&pm_config);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to configure power management, err:%d", err));
    sleep_diag_init(); /* no-op unless CONFIG_HOMECADIA_SLEEP_DIAG */
#endif

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    /* Thread diagnostics with all counters (attach attempts, parent changes,
     * MAC retries), so a controller read shows whether the sensor keeps losing
     * its parent. The SDK accepts only none or all four features
     * (thread-network-diagnostics-server/CodegenIntegration.cpp:57). Nothing
     * marks the counters dirty, so they cost no reports; a read (interview)
     * fetches them. */
    {
        cluster_t *tnd = cluster::get(endpoint::get(node, 0), chip::app::Clusters::ThreadNetworkDiagnostics::Id);
        ABORT_APP_ON_FAILURE(tnd != nullptr, ESP_LOGE(TAG, "No Thread diagnostics cluster on the root endpoint"));
        cluster::thread_network_diagnostics::feature::packet_counts::add(tnd);
        cluster::thread_network_diagnostics::feature::error_counts::add(tnd);
        cluster::thread_network_diagnostics::feature::mle_counts::add(tnd);
        cluster::thread_network_diagnostics::feature::mac_counts::add(tnd);
    }

    /* Temperature sensor endpoint (MeasuredValue in 0.01°C; SHT40 range) */
    endpoint::temperature_sensor::config_t temp_cfg;
    temp_cfg.temperature_measurement.min_measured_value = nullable<int16_t>(-4000);
    temp_cfg.temperature_measurement.max_measured_value = nullable<int16_t>(12500);
    endpoint_t *temp_ep = endpoint::temperature_sensor::create(node, &temp_cfg, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(temp_ep != nullptr, ESP_LOGE(TAG, "Failed to create temperature endpoint"));

    /* Humidity sensor endpoint (MeasuredValue in 0.01 %RH) */
    endpoint::humidity_sensor::config_t hum_cfg;
    hum_cfg.relative_humidity_measurement.min_measured_value = nullable<uint16_t>(0);
    hum_cfg.relative_humidity_measurement.max_measured_value = nullable<uint16_t>(10000);
    endpoint_t *hum_ep = endpoint::humidity_sensor::create(node, &hum_cfg, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(hum_ep != nullptr, ESP_LOGE(TAG, "Failed to create humidity endpoint"));

    /* Power source endpoint, battery feature. BatPercentRemaining and
     * BatVoltage are optional attributes, created explicitly below. */
    endpoint::power_source::config_t ps_cfg;
    ps_cfg.power_source.status =
        chip::to_underlying(chip::app::Clusters::PowerSource::PowerSourceStatusEnum::kActive);
    ps_cfg.power_source.order = 0;
    snprintf(ps_cfg.power_source.description, sizeof(ps_cfg.power_source.description), "2000mAh LiPo");
    ps_cfg.power_source.feature_flags = cluster::power_source::feature::battery::get_id();
    ps_cfg.power_source.features.battery.bat_charge_level =
        chip::to_underlying(chip::app::Clusters::PowerSource::BatChargeLevelEnum::kOk);
    ps_cfg.power_source.features.battery.bat_replaceability =
        chip::to_underlying(chip::app::Clusters::PowerSource::BatReplaceabilityEnum::kUserReplaceable);
    endpoint_t *ps_ep = endpoint::power_source::create(node, &ps_cfg, ENDPOINT_FLAG_NONE, nullptr);
    ABORT_APP_ON_FAILURE(ps_ep != nullptr, ESP_LOGE(TAG, "Failed to create power source endpoint"));

    cluster_t *ps_cluster = cluster::get(ps_ep, chip::app::Clusters::PowerSource::Id);
    ABORT_APP_ON_FAILURE(ps_cluster != nullptr, ESP_LOGE(TAG, "Failed to get power source cluster"));
    cluster::power_source::attribute::create_bat_percent_remaining(
        ps_cluster, nullable<uint8_t>(), nullable<uint8_t>(0), nullable<uint8_t>(200));
    cluster::power_source::attribute::create_bat_voltage(
        ps_cluster, nullable<uint32_t>(), nullable<uint32_t>(0), nullable<uint32_t>(4500));

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);
#endif
    err = settings_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Settings load failed (%s), using defaults", esp_err_to_name(err));
    }

    err = led_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LED init failed: %s", esp_err_to_name(err));
    }
    err = display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s — continuing headless", esp_err_to_name(err));
    }
    err = ui_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UI init failed: %s — continuing without input", esp_err_to_name(err));
    }
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    /* Not paired to any controller yet: put the onboarding QR on the screen,
     * and start the unpaired-sleep check so the uptime cap holds even if no
     * window or fail-safe event ever arrives. */
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
        show_commissioning_screen();
        chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) { schedule_unpaired_sleep_check(); });
    }
    sensor_loop_endpoints_t eps = {
        .temperature_endpoint_id = endpoint::get_id(temp_ep),
        .humidity_endpoint_id = endpoint::get_id(hum_ep),
        .power_source_endpoint_id = endpoint::get_id(ps_ep),
    };
    err = sensor_loop_start(&eps);
    if (err != ESP_OK) {
        /* Keep the node up for commissioning/bench tests without the sensor wired. */
        ESP_LOGE(TAG, "Sensor loop not running (%s)", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "homecadia sensor-01 up (poll %ds, report on ≥%.1f°C / ≥%.0f%%RH delta)",
             SENSOR_POLL_INTERVAL_S, REPORT_DELTA_TEMP_C, REPORT_DELTA_RH_PCT);
#if CONFIG_HOMECADIA_UNPAIRED_SLEEP_TEST_S > 0
    ESP_LOGW(TAG, "TEST BUILD: unpaired sleep forced %ds after boot", CONFIG_HOMECADIA_UNPAIRED_SLEEP_TEST_S);
    chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) {
        CHIP_ERROR e = chip::DeviceLayer::SystemLayer().StartTimer(
            chip::System::Clock::Seconds16(CONFIG_HOMECADIA_UNPAIRED_SLEEP_TEST_S), unpaired_sleep_check,
            reinterpret_cast<void *>(1));
        if (e != CHIP_NO_ERROR) {
            ESP_LOGE(TAG, "test sleep timer: %" CHIP_ERROR_FORMAT, e.Format());
        }
    });
#endif
}
