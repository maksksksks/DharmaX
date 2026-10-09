#include "svc_screen_sleep.h"
#include "bsp_display.h"
#include "svc_auto_brightness.h"
#include "esp_log.h"
#include "lvgl.h"
#include "screen_aod_logic.h"

static const char *TAG = "SVC_SLEEP";

#define SCREEN_SLEEP_IDLE_TIMEOUT_SEC 20   // совпадает с подписью в Settings ("Off after 20s idle")
#define SCREEN_SLEEP_FALLBACK_BRIGHTNESS 50 // если авто-яркость выключена, во сколько % будить экран

static bool s_enabled = true;
static bool s_sleeping = false;
static uint32_t s_idle_seconds = 0;
static lv_timer_t *s_idle_timer = NULL;

static void idle_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_enabled || s_sleeping) return;

    s_idle_seconds++;
    if (s_idle_seconds >= SCREEN_SLEEP_IDLE_TIMEOUT_SEC) {
        svc_screen_sleep_force_sleep();
    }
}

void svc_screen_sleep_init(void)
{
    s_idle_timer = lv_timer_create(idle_timer_cb, 1000, NULL);
    ESP_LOGI(TAG, "Screen sleep service initialized (idle timeout=%ds)", SCREEN_SLEEP_IDLE_TIMEOUT_SEC);
}

void svc_screen_sleep_wake_up(void)
{
    s_idle_seconds = 0;
    if (!s_sleeping) return;

    screen_aod_exit();

    s_sleeping = false;
    if (svc_auto_brightness_is_enabled()) {
        svc_auto_brightness_resume(true);
    } else {
        bsp_display_set_brightness(SCREEN_SLEEP_FALLBACK_BRIGHTNESS);
    }
    ESP_LOGI(TAG, "Screen woken up");
}

void svc_screen_sleep_force_sleep(void)
{
    if (!s_enabled || s_sleeping) return;

    screen_aod_enter();
    
    bsp_display_set_brightness(80); 

    s_sleeping = true;
    svc_auto_brightness_suspend();
    ESP_LOGI(TAG, "Screen put to sleep");
}

void svc_screen_sleep_set_enabled(bool enabled)
{
    s_enabled = enabled;
    if (!enabled && s_sleeping) {
        svc_screen_sleep_wake_up();
    }
}

bool svc_screen_sleep_is_enabled(void) { return s_enabled; }
bool svc_screen_sleep_is_sleeping(void) { return s_sleeping; }
