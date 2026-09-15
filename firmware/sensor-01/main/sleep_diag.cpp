/* Light-sleep diagnostics for the sensor's idle current (docs/field-notes.md
 * section 21). Counts every automatic light-sleep request, what IDF did with
 * it, what ended the sleep, and which power domains each sleep actually
 * powered down, and shows the result on the DIAG screen so it can be read with
 * no USB host attached (a host keeps the chip awake,
 * CONFIG_USJ_NO_AUTO_LS_ON_CONNECTION).
 *
 * Hooks: esp_pm light-sleep callbacks (esp_pm.h, CONFIG_PM_LIGHT_SLEEP_CALLBACKS).
 * In IDF v5.5.5 pm_impl.c vApplicationSleep calls the enter callback with the
 * requested sleep before comparing it to CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP,
 * and the exit callback with the time actually slept (0 when it did not try). */
#include "sleep_diag.h"

#include "sdkconfig.h"

#if CONFIG_HOMECADIA_SLEEP_DIAG

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "display.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_private/esp_sleep_internal.h"
#include "esp_private/sleep_retention.h"
#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#endif
#include "freertos/task.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "sensor_loop.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"

static const char *TAG = "sleep_diag";

/* The threshold vApplicationSleep applies before attempting a sleep. */
#define MIN_SLEEP_US ((int64_t)CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP * portTICK_PERIOD_MS * 1000)

static struct {
    uint32_t calls;                           /* enter callback runs */
    uint32_t req_short, req_3_10, req_10_100, req_100; /* requested sleep, ms */
    uint32_t tried;                           /* request >= MIN_SLEEP_US */
    uint32_t slept_lt1, slept_1_3, slept_ge3; /* time actually asleep, ms */
    uint32_t early;                           /* woke >1 ms before the request */
    uint32_t c_timer, c_gpio, c_other, c_none;
    uint32_t other_bits;                      /* OR of any other cause bits */
    uint32_t sw_low, enc_low;                 /* input levels just after wake */
    uint64_t slept_us;
} s;
static int64_t s_req_us;

/* Which domains and clocks each light sleep actually powers down. IDF writes
 * the applied flags into this context when CONFIG_ESP_SLEEP_DEBUG is set
 * (sleep_modes.c). Bits: esp_private/esp_pmu.h PMU_SLEEP_PD_* -- TOP 0,
 * MODEM 2, HP_PERIPH 3, CPU 4, XTAL 10, RC_FAST 11, LP_PERIPH 14. A bit set
 * means that part was powered down. GPIO3 was ruled out 2026-09-15: its
 * IO_MUX register read 0x1802 (sleep select on, sleep output/pulls off). */
static esp_sleep_context_t s_ctx;
static uint32_t s_flags_and = 0xffffffff, s_flags_or, s_flags_n;
static uint64_t s_modem_on_us, s_xtal_on_us; /* sleep time with that part left powered */

static esp_err_t IRAM_ATTR on_enter(int64_t sleep_time_us, void *arg)
{
    s.calls++;
    s_req_us = sleep_time_us;
    if (sleep_time_us < MIN_SLEEP_US) {
        s.req_short++;
    } else if (sleep_time_us < 10000) {
        s.req_3_10++;
    } else if (sleep_time_us < 100000) {
        s.req_10_100++;
    } else {
        s.req_100++;
    }
    return ESP_OK;
}

static esp_err_t IRAM_ATTR on_exit(int64_t slept_us, void *arg)
{
    if (s_req_us < MIN_SLEEP_US) {
        return ESP_OK; /* IDF did not attempt a sleep */
    }
    s.tried++;
    s.slept_us += slept_us;
    s_flags_and &= s_ctx.sleep_flags;
    s_flags_or |= s_ctx.sleep_flags;
    s_flags_n++;
    if (!(s_ctx.sleep_flags & BIT(2))) { /* PMU_SLEEP_PD_MODEM clear: modem domain stayed on */
        s_modem_on_us += slept_us;
    }
    if (!(s_ctx.sleep_flags & BIT(10))) { /* PMU_SLEEP_PD_XTAL clear: 40 MHz XTAL stayed on */
        s_xtal_on_us += slept_us;
    }
    if (slept_us < 1000) {
        s.slept_lt1++;
    } else if (slept_us < 3000) {
        s.slept_1_3++;
    } else {
        s.slept_ge3++;
    }
    if (slept_us + 1000 < s_req_us) {
        s.early++;
    }
    uint32_t causes = esp_sleep_get_wakeup_causes();
    const uint32_t timer = 1UL << ESP_SLEEP_WAKEUP_TIMER, gpio = 1UL << ESP_SLEEP_WAKEUP_GPIO;
    if (causes == 0) {
        s.c_none++;
    }
    if (causes & timer) {
        s.c_timer++;
    }
    if (causes & gpio) {
        s.c_gpio++;
    }
    if (causes & ~(timer | gpio)) {
        s.c_other++;
        s.other_bits |= causes & ~(timer | gpio);
    }
    if (gpio_get_level((gpio_num_t)ENC_PIN_SW) == 0) {
        s.sw_low++;
    }
    if (gpio_get_level((gpio_num_t)ENC_PIN_A) == 0 || gpio_get_level((gpio_num_t)ENC_PIN_B) == 0) {
        s.enc_low++;
    }
    return ESP_OK;
}

/* The dial cannot open the DIAG view on battery: its edge interrupts cannot
 * wake the chip from light sleep, and CONFIG_PM_SLP_DISABLE_GPIO disables the
 * pin inputs while asleep (found 2026-09-15). So redraw it on a timer. */
#define REDRAW_PERIOD_S 60

static void redraw_cb(void *arg)
{
    display_diag_t d = {};
    sensor_readings_t r = sensor_loop_get_readings();
    d.battery_pct = r.battery_pct;
    d.battery_mv = r.battery_mv;
    d.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    d.fw_version = esp_app_get_description()->version;
    display_show_diagnostics(&d);
}


void sleep_diag_init(void)
{
    esp_sleep_set_sleep_context(&s_ctx);

    static esp_timer_handle_t redraw;
    const esp_timer_create_args_t targs = {
        .callback = redraw_cb,
        .name = "sleep_diag_redraw",
    };
    if (esp_timer_create(&targs, &redraw) == ESP_OK) {
        esp_timer_start_periodic(redraw, (uint64_t)REDRAW_PERIOD_S * 1000000ULL);
    }

    esp_pm_sleep_cbs_register_config_t cfg = {};
    cfg.enter_cb = on_enter;
    cfg.exit_cb = on_exit;
    esp_err_t err = esp_pm_light_sleep_register_cbs(&cfg);
    ESP_LOGW(TAG, "light-sleep counters %s", err == ESP_OK ? "registered" : esp_err_to_name(err));
}

int sleep_diag_format(char lines[][SLEEP_DIAG_COLS])
{
    double up = esp_timer_get_time() / 1e6;
    if (up < 1) {
        up = 1;
    }
    int k = 0;
    snprintf(lines[k++], SLEEP_DIAG_COLS, "up %.0fs calls %.0f/s tried %.0f/s asleep %.0f%%", up, s.calls / up,
             s.tried / up, s.slept_us / 1e4 / up);
    snprintf(lines[k++], SLEEP_DIAG_COLS, "req/s <3ms %.0f 3-10 %.0f 10-100 %.1f >100 %.1f", s.req_short / up,
             s.req_3_10 / up, s.req_10_100 / up, s.req_100 / up);
    snprintf(lines[k++], SLEEP_DIAG_COLS, "slept/s <1ms %.0f 1-3 %.0f >=3 %.1f early %.0f", s.slept_lt1 / up,
             s.slept_1_3 / up, s.slept_ge3 / up, s.early / up);
    snprintf(lines[k++], SLEEP_DIAG_COLS, "wake/s tmr %.0f gpio %.0f oth %.0f none %.0f", s.c_timer / up,
             s.c_gpio / up, s.c_other / up, s.c_none / up);
    snprintf(lines[k++], SLEEP_DIAG_COLS, "other bits 0x%lx at wake/s SW low %.0f ENC %.0f",
             (unsigned long)s.other_bits, s.sw_low / up, s.enc_low / up);
    snprintf(lines[k++], SLEEP_DIAG_COLS, "pd flags last %08lx n %lu", (unsigned long)s_ctx.sleep_flags,
             (unsigned long)s_flags_n);
    snprintf(lines[k++], SLEEP_DIAG_COLS, "pd in every sleep (AND) %08lx", (unsigned long)(s_flags_n ? s_flags_and : 0));
    snprintf(lines[k++], SLEEP_DIAG_COLS, "pd in any sleep  (OR)  %08lx", (unsigned long)s_flags_or);
    double slept = s.slept_us ? (double)s.slept_us : 1.0;
    snprintf(lines[k++], SLEEP_DIAG_COLS, "of sleep time: modem on %.0f%% xtal on %.0f%%", 100.0 * s_modem_on_us / slept,
             100.0 * s_xtal_on_us / slept);
#if CONFIG_BT_ENABLED
    int bt = (int)esp_bt_controller_get_status(); /* 0 idle, 1 inited, 2 enabled */
#else
    int bt = -1;
#endif
    sleep_retention_module_bitmap_t ri = sleep_retention_get_inited_modules();
    sleep_retention_module_bitmap_t rc = sleep_retention_get_created_modules();
    /* bits 28 BLE_MAC, 29 BT_BB, 30 802154_MAC (soc/retention_periph_defs.h) */
    snprintf(lines[k++], SLEEP_DIAG_COLS, "bt %d nimble %d ret i %08lx c %08lx", bt,
             xTaskGetHandle("nimble_host") != nullptr, (unsigned long)ri.bitmap[0], (unsigned long)rc.bitmap[0]);
    return k;
}

#else

void sleep_diag_init(void) {}
int sleep_diag_format(char lines[][SLEEP_DIAG_COLS])
{
    (void)lines;
    return 0;
}

#endif
