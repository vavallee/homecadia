#include "battery.h"

#include "app_config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

#define DIVIDER_RATIO 2  /* 1M : 1M */
#define SAMPLES       8

/* The computed battery voltage sits a constant ~340mV below BAT+. Measured
 * 2026-09-12 against a PPK2 source, BAT+ metered in-circuit, DIAG view read
 * after a full poll:  BAT+ 3.36V -> 3020mV,  BAT+ 3.97V -> 3640mV  (slope 1.02,
 * so an offset, not a gain error). Cause inferred, not measured: ~0.34uA into
 * the ADC input across the divider's 500k source impedance. docs/bringup.md,
 * ADC calibration row. */
#define VBAT_OFFSET_MV 340

static adc_unit_t s_unit;
static adc_cali_handle_t s_cali;
static adc_channel_t s_channel;
#if CONFIG_PM_ENABLE
/* No light sleep may straddle the ADC sequence. Creating the unit changes the
 * sleep power-domain config (modem, and TOP when peripheral power-down is on)
 * and deleting it changes it back; with CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_
 * LIGHT_SLEEP=y a sleep landing inside that window stops the chip waking
 * ~480 s in -- bisected to this read on 2026-09-15 (docs/field-notes.md
 * section 21). IDF has no ADC sleep retention yet (esp_adc/adc_oneshot.c,
 * TODO IDF-8475). Costs ~10 ms of awake time per 120 s poll. */
static esp_pm_lock_handle_t s_no_sleep;
#endif

/* The ADC unit exists only for the length of one reading. On the C6 a live
 * oneshot unit keeps the modem power domain on through every light sleep:
 * adc_oneshot_new_unit() calls esp_sleep_pd_config(ESP_PD_DOMAIN_MODEM, ON)
 * because the ADC front end sits in that domain (ADC_LL_ADC_FE_ON_MODEM_DOMAIN,
 * hal/esp32c6/include/hal/adc_ll.h), and only adc_oneshot_del_unit() sets it
 * back to OFF (esp_adc/adc_oneshot.c, IDF v5.5.5). Created once at boot, it
 * held the modem domain up for 100% of sleep time -- measured 2026-09-15 with
 * the sleep-diag image (CONFIG_HOMECADIA_SLEEP_DIAG). The calibration handle
 * is only eFuse coefficients and does not need a live unit, so it is kept. */
esp_err_t battery_init(void)
{
    esp_err_t err = adc_oneshot_io_to_channel(VBAT_ADC_GPIO, &s_unit, &s_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d is not an ADC pin", VBAT_ADC_GPIO);
        return err;
    }

#if CONFIG_PM_ENABLE
    err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "battery_adc", &s_no_sleep);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no-sleep lock: %s", esp_err_to_name(err));
        return err;
    }
#endif

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = s_unit,
        .chan = s_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No ADC calibration (%s); readings will be raw-scaled", esp_err_to_name(err));
        s_cali = nullptr;
    }
    return ESP_OK;
}

esp_err_t battery_read_mv(uint32_t *out_mv)
{
#if CONFIG_PM_ENABLE
    esp_pm_lock_acquire(s_no_sleep);
#endif
    adc_oneshot_unit_handle_t adc = nullptr;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = s_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &adc);
    if (err != ESP_OK) {
#if CONFIG_PM_ENABLE
        esp_pm_lock_release(s_no_sleep);
#endif
        return err;
    }

    /* 12dB attenuation: full-scale ~3.3V, Vbat/2 tops out at 2.1V. */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(adc, s_channel, &chan_cfg);

    /* The 100nF cap holds the divider node steady (source is always connected),
     * but give the ADC input a moment after (re)config before sampling.
     * The 2026-09-02 "within 1.2%" check once cited here was a false pass
     * (docs/bringup.md, divider row); the real error is VBAT_OFFSET_MV. */
    int sum = 0;
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(VBAT_ADC_SETTLE_MS));
        for (int i = 0; i < SAMPLES; i++) {
            int raw = 0;
            err = adc_oneshot_read(adc, s_channel, &raw);
            if (err != ESP_OK) {
                break;
            }
            sum += raw;
        }
    }
    adc_oneshot_del_unit(adc); /* releases the modem-domain hold before the next sleep */
#if CONFIG_PM_ENABLE
    esp_pm_lock_release(s_no_sleep);
#endif
    if (err != ESP_OK) {
        return err;
    }
    int raw_avg = sum / SAMPLES;

    int mv_at_pin;
    if (s_cali) {
        esp_err_t err = adc_cali_raw_to_voltage(s_cali, raw_avg, &mv_at_pin);
        if (err != ESP_OK) {
            return err;
        }
    } else {
        mv_at_pin = raw_avg * 3300 / 4095;
    }

    /* 0 means no divider / open input: report 0 rather than a phantom 340mV. */
    *out_mv = mv_at_pin > 0 ? (uint32_t)mv_at_pin * DIVIDER_RATIO + VBAT_OFFSET_MV : 0;
    return ESP_OK;
}

/* LiPo open-circuit voltage → percent, linear interpolation between points. */
static const struct {
    uint16_t mv;
    uint8_t pct;
} k_ocv_lut[] = {
    {4200, 100}, {4100, 94}, {4000, 85}, {3950, 80}, {3900, 74},
    {3850, 68},  {3800, 60}, {3750, 51}, {3700, 42}, {3650, 32},
    {3600, 20},  {3550, 12}, {3500, 7},  {3450, 4},  {3400, 2},
    {3300, 1},   {3000, 0},
};

uint8_t battery_percent_from_mv(uint32_t mv)
{
    if (mv >= k_ocv_lut[0].mv) {
        return 100;
    }
    const int n = sizeof(k_ocv_lut) / sizeof(k_ocv_lut[0]);
    for (int i = 1; i < n; i++) {
        if (mv >= k_ocv_lut[i].mv) {
            uint32_t span_mv = k_ocv_lut[i - 1].mv - k_ocv_lut[i].mv;
            uint32_t span_pct = k_ocv_lut[i - 1].pct - k_ocv_lut[i].pct;
            return k_ocv_lut[i].pct + (mv - k_ocv_lut[i].mv) * span_pct / span_mv;
        }
    }
    return 0;
}
