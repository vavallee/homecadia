#include "battery.h"

#include "app_config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

#define DIVIDER_RATIO 2  /* 1M : 1M */
#define SAMPLES       8

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static adc_channel_t s_channel;

esp_err_t battery_init(void)
{
    adc_unit_t unit;
    esp_err_t err = adc_oneshot_io_to_channel(VBAT_ADC_GPIO, &unit, &s_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d is not an ADC pin", VBAT_ADC_GPIO);
        return err;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) {
        return err;
    }

    /* 12dB attenuation: full-scale ~3.3V, Vbat/2 tops out at 2.1V. */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_oneshot_config_channel(s_adc, s_channel, &chan_cfg);
    if (err != ESP_OK) {
        return err;
    }

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = unit,
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
    /* The 100nF cap holds the divider node steady (source is always connected),
     * but give the ADC input a moment after (re)config before sampling.
     * HW-VERIFY: confirm reading is stable and matches multimeter ±5%. */
    vTaskDelay(pdMS_TO_TICKS(VBAT_ADC_SETTLE_MS));

    int sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(s_adc, s_channel, &raw);
        if (err != ESP_OK) {
            return err;
        }
        sum += raw;
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

    *out_mv = (uint32_t)mv_at_pin * DIVIDER_RATIO;
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
