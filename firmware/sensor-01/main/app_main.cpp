/* homecadia sensor-01 — Matter-over-Thread temperature/humidity sensor.
 *
 * Endpoints: temperature sensor, humidity sensor, power source (battery).
 * SHT40 polled every SENSOR_POLL_INTERVAL_S; attributes update on delta
 * (sensor_loop.cpp). Structure follows the esp-matter icd_app example (v1.6).
 */

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#endif

#include <esp_matter.h>
#include <esp_matter_ota.h>

#include <common_macros.h>
#include <app_config.h>
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
 * the serial console via the stack's own PrintOnboardingCodes). */
static void show_commissioning_screen(void)
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
    display_show_commissioning(qr, manual);
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete: {
        ESP_LOGI(TAG, "Commissioning complete");
        led_set_commissioning(false);
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
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kThreadConnectivityChange:
        ESP_LOGI(TAG, "Thread connectivity change: %d", event->ThreadConnectivityChange.Result);
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved: {
        ESP_LOGI(TAG, "Fabric removed");
        /* Last controller un-paired us: reopen the commissioning window so the
         * device can be re-adopted without a manual factory reset. */
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
            chip::CommissioningWindowManager &mgr = chip::Server::GetInstance().GetCommissioningWindowManager();
            if (!mgr.IsCommissioningWindowOpen()) {
                CHIP_ERROR err = mgr.OpenBasicCommissioningWindow(k_commissioning_window_timeout,
                                                                  chip::CommissioningWindowAdvertisement::kDnssdOnly);
                if (err != CHIP_NO_ERROR) {
                    ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                }
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
#endif

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

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

    /* Not paired to any controller yet: put the onboarding QR on the screen. */
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0) {
        show_commissioning_screen();
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
}
