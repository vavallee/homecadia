#include "bench_selftest.h"

#if CONFIG_HOMECADIA_BENCH_SELFTEST

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "pinprobe.h"

static const char *TAG = "harness";

/* A pull-up/pull-down probe on a live I2C bus is itself a START condition:
 * SDA falling while SCL is high. An attached slave then waits for clocks that
 * never come and holds SDA low -- verified 2026-08-25, when the probe above
 * left the SHT40 wedged and its own init then read both lines held LOW.
 * Standard recovery: nine SCL clocks with SDA released, then a STOP. */
static void i2c_bus_recover(int sda, int scl)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << sda) | (1ULL << scl),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    gpio_set_level((gpio_num_t)sda, 1);
    gpio_set_level((gpio_num_t)scl, 1);
    esp_rom_delay_us(10);
    for (int i = 0; i < 9; i++) {
        gpio_set_level((gpio_num_t)scl, 0);
        esp_rom_delay_us(5);
        gpio_set_level((gpio_num_t)scl, 1);
        esp_rom_delay_us(5);
    }
    /* STOP: SDA rises while SCL is high. */
    gpio_set_level((gpio_num_t)sda, 0);
    esp_rom_delay_us(5);
    gpio_set_level((gpio_num_t)scl, 1);
    esp_rom_delay_us(5);
    gpio_set_level((gpio_num_t)sda, 1);
    esp_rom_delay_us(5);

    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io);
    ESP_LOGI(TAG, "  I2C bus recovered (9 clocks + STOP) after probing SDA/SCL");
}

/* Runs first thing in app_main(), before board_rf_switch_init() has even
 * finished being the only thing that has touched a pin. Order matters: a pin a
 * driver already owns reports the driver, not the wire (field-notes.md s16).
 *
 * Outputs get a verdict -- a line the C6 drives high must read high, and there
 * is no innocent explanation when it does not. Inputs are reported as facts:
 * "held HIGH" on SDA is the driver board's own pull-up, "floating" on BUSY is a
 * panel that is not plugged in, and neither is a fault. An earlier version
 * attached expectations to inputs and flagged healthy boards. */

/* Cross-coupling scan: two nets that should be independent but move together
 * are bridged -- a solder bridge, a misplaced jumper, a wire in the wrong
 * breadboard row. Drive each candidate high then low and read every other pin
 * both times; report any pin that follows. Pre-init only: nothing else drives
 * anything yet, so a follower is a wire, not a peripheral.
 *
 * Found 2026-08-25: SDA read the board pull-up until display_init() drove DC
 * low, then followed it -- the D4 net was tied to D3. A per-pin probe cannot
 * see that; only driving one and watching the others can. */

/* How hard is a coupling? A digital 0/1 cannot say: a wire and a 20k leak
 * both read as "follows". Charge the follower high, release it with no pull,
 * hold the driver low and time the fall. Pad plus board capacitance is tens
 * of pF, so a wire falls in well under a microsecond, a resistive path in
 * tens to hundreds of microseconds, and a line with no DC path to the driver
 * sits there for milliseconds or never falls at all. */
static int64_t fall_time_us(int driver, int follower)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << follower,
        .mode = GPIO_MODE_INPUT_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level((gpio_num_t)follower, 1);
    esp_rom_delay_us(200);

    gpio_set_level((gpio_num_t)driver, 0);
    io.mode = GPIO_MODE_INPUT;
    gpio_config(&io); /* release: no pull, only the coupling can move it */

    int64_t t0 = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)follower) != 0) {
        if (esp_timer_get_time() - t0 > 20000) {
            return -1; /* did not fall within 20 ms */
        }
    }
    return esp_timer_get_time() - t0;
}



/* For followers on ADC-capable pads (C6: GPIO0-6 = ADC1 CH0-6), read the
 * actual voltage with the internal ~45k pull-up on and the driver held low.
 * V = 3.3 * R / (45k + R), so R = 45k * V / (3.3 - V): a wire reads near 0 mV,
 * a 10k path about 600 mV, a 100k leak about 2.3 V. This is the one reading
 * in this file that is quantitative; the fall-time and 0/1 checks are not. */
static void measure_follower_mv(int driver, int follower)
{
    if (follower < 0 || follower > 6) {
        return;
    }
    adc_oneshot_unit_handle_t adc = nullptr;
    adc_oneshot_unit_init_cfg_t ucfg = {.unit_id = ADC_UNIT_1};
    if (adc_oneshot_new_unit(&ucfg, &adc) != ESP_OK) {
        return;
    }
    adc_oneshot_chan_cfg_t ccfg = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    adc_oneshot_config_channel(adc, (adc_channel_t)follower, &ccfg);

    gpio_config_t dio = {
        .pin_bit_mask = 1ULL << driver,
        .mode = GPIO_MODE_INPUT_OUTPUT,
    };
    gpio_config(&dio);
    gpio_set_level((gpio_num_t)driver, 0);
    /* The ADC path disables the digital pull-up on the pad, so supply it via
     * the LP/RTC pull-up register path: gpio_pullup_en works on analog pads. */
    gpio_pullup_en((gpio_num_t)follower);
    esp_rom_delay_us(500);

    int raw = 0;
    adc_oneshot_read(adc, (adc_channel_t)follower, &raw);
    int mv = raw * 3300 / 4095; /* uncalibrated; +-10% is plenty here */
    int r_k = mv < 3200 ? (45 * mv) / (3300 - mv) : 9999;
    ESP_LOGE(TAG, "    measured: GPIO%d at %d mV with GPIO%d low -> ~%dk path (0 = wire, >100k = leak)",
             follower, mv, driver, r_k);

    gpio_set_level((gpio_num_t)driver, 1);
    adc_oneshot_del_unit(adc);
    gpio_config_t rio = {
        .pin_bit_mask = (1ULL << follower) | (1ULL << driver),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&rio);
}

static void coupling_scan(void)
{
    const struct {
        const char *name;
        int gpio;
        bool drive; /* false: read-only (a peripheral output we must not fight) */
    } pins[] = {
        {"EPD RST ", EPD_PIN_RST, true},   {"EPD CS  ", EPD_PIN_CS, true},
        {"EPD DC  ", EPD_PIN_DC, true},    {"EPD SCK ", EPD_PIN_SCK, true},
        {"EPD MOSI", EPD_PIN_MOSI, true},  {"EPD BUSY", EPD_PIN_BUSY, false},
        {"I2C SDA ", SHT40_I2C_SDA, true}, {"I2C SCL ", SHT40_I2C_SCL, true},
        {"LED     ", LED_PIN, true},       {"ENC A   ", ENC_PIN_A, true},
        {"ENC B   ", ENC_PIN_B, true},     {"ENC SW  ", ENC_PIN_SW, true},
        {"VBAT ADC", VBAT_ADC_GPIO, true},
    };
    const int n = sizeof(pins) / sizeof(pins[0]);

    /* Everything a plain input with a weak pull-up, so an unconnected pin
     * reads a steady 1 and only a driven neighbour can move it. */
    for (int i = 0; i < n; i++) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << pins[i].gpio,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        gpio_config(&io);
    }
    esp_rom_delay_us(2000);

    int bridges = 0;
    for (int d = 0; d < n; d++) {
        if (!pins[d].drive) {
            continue;
        }
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << pins[d].gpio,
            .mode = GPIO_MODE_INPUT_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        gpio_config(&io);
        int hi[16], lo[16];
        gpio_set_level((gpio_num_t)pins[d].gpio, 1);
        esp_rom_delay_us(500);
        for (int r = 0; r < n; r++) {
            hi[r] = gpio_get_level((gpio_num_t)pins[r].gpio);
        }
        gpio_set_level((gpio_num_t)pins[d].gpio, 0);
        esp_rom_delay_us(500);
        for (int r = 0; r < n; r++) {
            lo[r] = gpio_get_level((gpio_num_t)pins[r].gpio);
        }
        gpio_set_level((gpio_num_t)pins[d].gpio, 1);
        io.mode = GPIO_MODE_INPUT;
        gpio_config(&io);

        for (int r = 0; r < n; r++) {
            if (r != d && hi[r] == 1 && lo[r] == 0) {
                /* Re-drive d low for the timing measurement, then release. */
                gpio_config_t dio = {
                    .pin_bit_mask = 1ULL << pins[d].gpio,
                    .mode = GPIO_MODE_INPUT_OUTPUT,
                };
                gpio_config(&dio);
                int64_t us = fall_time_us(pins[d].gpio, pins[r].gpio);
                gpio_set_level((gpio_num_t)pins[d].gpio, 1);
                dio.mode = GPIO_MODE_INPUT;
                dio.pull_up_en = GPIO_PULLUP_ENABLE;
                gpio_config(&dio);
                /* restore the follower's pull-up for the rest of the scan */
                gpio_config_t rio = {
                    .pin_bit_mask = 1ULL << pins[r].gpio,
                    .mode = GPIO_MODE_INPUT,
                    .pull_up_en = GPIO_PULLUP_ENABLE,
                };
                gpio_config(&rio);

                ESP_LOGW(TAG, "  coupled: %s GPIO%-2d follows %s GPIO%-2d (fall %lldus; <100k, not sized)",
                         pins[r].name, pins[r].gpio, pins[d].name, pins[d].gpio, us);
                measure_follower_mv(pins[d].gpio, pins[r].gpio);
                bridges++;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << pins[i].gpio,
            .mode = GPIO_MODE_INPUT,
        };
        gpio_config(&io);
    }
    if (bridges == 0) {
        ESP_LOGI(TAG, "  coupling scan: no pin follows any other");
    }
}

void bench_selftest(void)
{
    ESP_LOGW(TAG, "harness scan (nothing driven yet)");

    const struct {
        const char *name;
        int gpio;
    } outputs[] = {
        {"EPD RST ", EPD_PIN_RST},  {"EPD CS  ", EPD_PIN_CS},   {"EPD DC  ", EPD_PIN_DC},
        {"EPD SCK ", EPD_PIN_SCK},  {"EPD MOSI", EPD_PIN_MOSI}, {"LED     ", LED_PIN},
    };
    int faults = 0;
    for (auto &p : outputs) {
        pinprobe_drive_t d = pinprobe_drive_test(p.gpio);
        if (pinprobe_drive_ok(d)) {
            ESP_LOGI(TAG, "  %s GPIO%-2d drive hi=%d lo=%d  %s", p.name, p.gpio, d.hi, d.lo,
                     pinprobe_drive_verdict(d));
        } else {
            ESP_LOGE(TAG, "  %s GPIO%-2d drive hi=%d lo=%d  %s", p.name, p.gpio, d.hi, d.lo,
                     pinprobe_drive_verdict(d));
            faults++;
        }
    }

    const struct {
        const char *name;
        int gpio;
        const char *note;
    } inputs[] = {
        {"EPD BUSY", EPD_PIN_BUSY, "idle panel: held LOW; no panel: floating"},
        {"I2C SDA ", SHT40_I2C_SDA, "sensor module pull-up: held HIGH; floating = no sensor power"},
        {"I2C SCL ", SHT40_I2C_SCL, "sensor module pull-up: held HIGH; held LOW = no clock possible"},
        {"ENC A   ", ENC_PIN_A, "floating at rest (switch open); held LOW mid-detent"},
        {"ENC B   ", ENC_PIN_B, "floating at rest (switch open); held LOW mid-detent"},
        {"ENC SW  ", ENC_PIN_SW, "held HIGH by the C6 JTAG pull-up until MTCK is soldered"},
        {"VBAT ADC", VBAT_ADC_GPIO, "floating until the divider is built"},
    };
    for (auto &p : inputs) {
        pinprobe_line_t s = pinprobe_line_state(p.gpio);
        ESP_LOGI(TAG, "  %s GPIO%-2d %-10s (%s)", p.name, p.gpio, pinprobe_line_name(s), p.note);
    }

    coupling_scan();
    i2c_bus_recover(SHT40_I2C_SDA, SHT40_I2C_SCL);

    if (faults) {
        ESP_LOGE(TAG, "%d output(s) cannot be driven -- fix the wiring before reading anything else", faults);
    } else {
        ESP_LOGW(TAG, "all outputs follow the driver");
    }
}

void bench_probe_i2c(const char *when)
{
    pinprobe_line_t sda = pinprobe_line_state(SHT40_I2C_SDA);
    pinprobe_line_t scl = pinprobe_line_state(SHT40_I2C_SCL);
    /* Raw level BEFORE the probe reconfigures the pin: shows what the current
     * owner (if any) leaves on it. Then the probe's own verdict. */
    int a_raw = gpio_get_level((gpio_num_t)ENC_PIN_A);
    int b_raw = gpio_get_level((gpio_num_t)ENC_PIN_B);
    pinprobe_line_t a = pinprobe_line_state(ENC_PIN_A);
    pinprobe_line_t b = pinprobe_line_state(ENC_PIN_B);
    ESP_LOGW(TAG, "lines %s: SDA %s, SCL %s | ENC A raw=%d %s, ENC B raw=%d %s", when,
             pinprobe_line_name(sda), pinprobe_line_name(scl), a_raw, pinprobe_line_name(a), b_raw,
             pinprobe_line_name(b));
}

static void enc_raw_task(void *)
{
    int a = gpio_get_level((gpio_num_t)ENC_PIN_A);
    int b = gpio_get_level((gpio_num_t)ENC_PIN_B);
    ESP_LOGW(TAG, "encoder raw: A=%d B=%d (at rest; both should be 1 with pull-ups)", a, b);
    for (;;) {
        int na = gpio_get_level((gpio_num_t)ENC_PIN_A);
        int nb = gpio_get_level((gpio_num_t)ENC_PIN_B);
        if (na != a || nb != b) {
            ESP_LOGW(TAG, "encoder raw: A=%d B=%d", na, nb);
            a = na;
            b = nb;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void bench_encoder_monitor_start(void)
{
    xTaskCreate(enc_raw_task, "enc_raw", 2048, nullptr, 2, nullptr);
}

#endif /* CONFIG_HOMECADIA_BENCH_SELFTEST */
