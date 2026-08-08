#include "sensor_loop.h"

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"

#include <esp_matter.h>
#include <esp_matter_attribute_utils.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>

#include "app_config.h"
#include "battery.h"
#include "display.h"
#include "settings.h"
#include "sht40.h"

static const char *TAG = "sensor_loop";

using namespace chip::app::Clusters;
using esp_matter::attribute::update;

static sensor_loop_endpoints_t s_eps;
static sht40_handle_t s_sht40;
static esp_timer_handle_t s_timer;

static sensor_readings_t s_readings;

/* Last values actually reported over Matter (delta reference). */
static float s_reported_temp_c = NAN;
static float s_reported_rh = NAN;
static uint8_t s_reported_bat_pct = 0xFF;
static unsigned s_polls_since_report;

static void report_matter(float temp_c, float rh, uint32_t bat_mv, uint8_t bat_pct)
{
    /* Attribute updates must run under the Matter stack lock; this runs in the
     * esp_timer task. One locked section batches all clusters into one wake.
     * With portMAX_DELAY the scoped lock only ever proceeds locked. */
    esp_matter::lock::ScopedChipStackLock stack_lock(portMAX_DELAY);

    esp_matter_attr_val_t val = esp_matter_nullable_int16((int16_t)lroundf(temp_c * 100.0f));
    update(s_eps.temperature_endpoint_id, TemperatureMeasurement::Id,
           TemperatureMeasurement::Attributes::MeasuredValue::Id, &val);

    val = esp_matter_nullable_uint16((uint16_t)lroundf(rh * 100.0f));
    update(s_eps.humidity_endpoint_id, RelativeHumidityMeasurement::Id,
           RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, &val);

    /* BatPercentRemaining is in half-percent units (0..200). */
    val = esp_matter_nullable_uint8(bat_pct * 2);
    update(s_eps.power_source_endpoint_id, PowerSource::Id,
           PowerSource::Attributes::BatPercentRemaining::Id, &val);

    val = esp_matter_nullable_uint32(bat_mv);
    update(s_eps.power_source_endpoint_id, PowerSource::Id,
           PowerSource::Attributes::BatVoltage::Id, &val);
}

static void poll_cb(void *arg)
{
    float temp_c, rh;
    esp_err_t err = sht40_read(s_sht40, &temp_c, &rh);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SHT40 read failed: %s", esp_err_to_name(err));
        return;
    }

    uint32_t bat_mv = 0;
    uint8_t bat_pct = 0;
    err = battery_read_mv(&bat_mv);
    if (err == ESP_OK) {
        bat_pct = battery_percent_from_mv(bat_mv);
    } else {
        ESP_LOGW(TAG, "battery read failed: %s", esp_err_to_name(err));
    }

    s_readings = {temp_c, rh, bat_mv, bat_pct, true};

    s_polls_since_report++;
    bool delta_hit = isnan(s_reported_temp_c) ||
                     fabsf(temp_c - s_reported_temp_c) >= REPORT_DELTA_TEMP_C ||
                     fabsf(rh - s_reported_rh) >= REPORT_DELTA_RH_PCT ||
                     (bat_pct + 1 < s_reported_bat_pct); /* battery only falls; 1% hysteresis */
    bool force = s_polls_since_report >= FORCE_REPORT_EVERY_N_POLLS;

    if (delta_hit || force) {
        report_matter(temp_c, rh, bat_mv, bat_pct);
        /* display refresh rides on the same delta gate as the radio report */
        display_show_readings(&s_readings);
        s_reported_temp_c = temp_c;
        s_reported_rh = rh;
        s_reported_bat_pct = bat_pct;
        s_polls_since_report = 0;
        ESP_LOGI(TAG, "reported %.2f°C %.1f%%RH bat %u%% (%lumV)%s", temp_c, rh, bat_pct,
                 (unsigned long)bat_mv, force ? " [forced]" : "");
    } else {
        ESP_LOGD(TAG, "no delta: %.2f°C %.1f%%RH bat %u%%", temp_c, rh, bat_pct);
    }
}

esp_err_t sensor_loop_start(const sensor_loop_endpoints_t *endpoints)
{
    s_eps = *endpoints;

    esp_err_t err = sht40_init(SHT40_I2C_SDA, SHT40_I2C_SCL, SHT40_I2C_ADDR, &s_sht40);
    if (err != ESP_OK) {
        /* HW-VERIFY: on real hardware this is fatal; without the sensor wired
         * (bench bringup) keep the node alive for commissioning tests. */
        ESP_LOGE(TAG, "SHT40 init failed: %s — sensor loop not started", esp_err_to_name(err));
        return err;
    }

    err = battery_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "battery gauge init failed: %s", esp_err_to_name(err));
    }

    const esp_timer_create_args_t targs = {
        .callback = poll_cb,
        .name = "sensor_poll",
    };
    err = esp_timer_create(&targs, &s_timer);
    if (err != ESP_OK) {
        return err;
    }

    poll_cb(nullptr); /* first reading immediately */
    return esp_timer_start_periodic(s_timer, (uint64_t)settings_get().poll_interval_s * 1000000ULL);
}

esp_err_t sensor_loop_set_poll_interval(uint16_t seconds)
{
    if (!s_timer) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_timer_stop(s_timer);
    ESP_LOGI(TAG, "poll interval -> %us", seconds);
    return esp_timer_start_periodic(s_timer, (uint64_t)seconds * 1000000ULL);
}

sensor_readings_t sensor_loop_get_readings(void)
{
    return s_readings;
}
