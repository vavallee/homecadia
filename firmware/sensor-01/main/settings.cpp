#include "settings.h"

#include "app_config.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "settings";
static const char *NVS_NS = "homecadia";

static app_settings_t s_settings = {
    .poll_interval_s = SENSOR_POLL_INTERVAL_S,
    .use_fahrenheit = false,
};

esp_err_t settings_init(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK; /* first boot: defaults */
    }
    if (err != ESP_OK) {
        return err;
    }
    uint16_t poll;
    if (nvs_get_u16(nvs, "poll_s", &poll) == ESP_OK && poll >= 30 && poll <= 3600) {
        s_settings.poll_interval_s = poll;
    }
    uint8_t f;
    if (nvs_get_u8(nvs, "fahrenheit", &f) == ESP_OK) {
        s_settings.use_fahrenheit = f != 0;
    }
    nvs_close(nvs);
    return ESP_OK;
}

app_settings_t settings_get(void)
{
    return s_settings;
}

esp_err_t settings_save(const app_settings_t *s)
{
    s_settings = *s;
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }
    nvs_set_u16(nvs, "poll_s", s->poll_interval_s);
    nvs_set_u8(nvs, "fahrenheit", s->use_fahrenheit ? 1 : 0);
    err = nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "saved: poll %us, units %s", s->poll_interval_s, s->use_fahrenheit ? "F" : "C");
    return err;
}
