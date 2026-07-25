#include "screen_aod_logic.h"
#include "ui.h"
#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include <time.h>
#include <math.h>

static const char *TAG = "SCREEN_AOD";

// Центр экрана — ЕДИНАЯ точка отсчёта
#define CX  (EXAMPLE_LCD_H_RES / 2)
#define CY  (EXAMPLE_LCD_V_RES / 2)

static lv_obj_t *s_prev_screen = NULL;
static lv_timer_t *s_tick_timer = NULL;
static bool s_in_aod = false;

static void aod_update_hands(void)
{
    time_t now;
    struct tm tm_now;
    time(&now);
    localtime_r(&now, &tm_now);

    int hour   = tm_now.tm_hour % 12;
    int minute = tm_now.tm_min;
    int second = tm_now.tm_sec;

    float hour_deg   = (hour + minute / 60.0f) * 30.0f;
    float minute_deg = (minute + second / 60.0f) * 6.0f;
    float second_deg = second * 6.0f;

    // Вращение стрелок (pivot уже в CX, CY)
    lv_obj_set_style_transform_rotation(ui_AodHourHand,   (int32_t)(hour_deg * 10),   LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(ui_AodMinuteHand, (int32_t)(minute_deg * 10), LV_PART_MAIN);

    // ★ Секундный кружок: абсолютная позиция через lv_obj_set_pos
    float rad = second_deg * (float)M_PI / 180.0f;
    int32_t dot_x = CX + (int32_t)(AOD_SECOND_TRACK_R * sinf(rad)) - AOD_SECOND_DOT_SIZE / 2;
    int32_t dot_y = CY - (int32_t)(AOD_SECOND_TRACK_R * cosf(rad)) - AOD_SECOND_DOT_SIZE / 2;
    lv_obj_set_pos(ui_AodSecondDot, dot_x, dot_y);
}

static void aod_tick_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    aod_update_hands();
}

void screen_aod_enter(void)
{
    if (s_in_aod) return;

    s_prev_screen = lv_screen_active();
    if (s_prev_screen == ui_ScreenAOD) s_prev_screen = ui_ScreenHome;

    if (ui_ScreenAOD == NULL) ui_ScreenAOD_screen_init();

    aod_update_hands();
    lv_screen_load_anim(ui_ScreenAOD, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);

    if (s_tick_timer == NULL)
        s_tick_timer = lv_timer_create(aod_tick_timer_cb, 1000, NULL);

    s_in_aod = true;
    ESP_LOGI(TAG, "Entered AOD watchface");
}

void screen_aod_exit(void)
{
    if (!s_in_aod) return;

    if (s_tick_timer != NULL) {
        lv_timer_del(s_tick_timer);
        s_tick_timer = NULL;
    }

    lv_obj_t *target = (s_prev_screen != NULL) ? s_prev_screen : ui_ScreenHome;
    if (target == ui_ScreenHome && ui_ScreenHome == NULL)
        ui_ScreenHome_screen_init();
    lv_screen_load_anim(target, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false);

    s_in_aod = false;
    ESP_LOGI(TAG, "Exited AOD watchface");
}

static void wrist_sleep_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id; (void)event_data;
    screen_aod_enter();
}

static void wrist_raise_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id; (void)event_data;
    screen_aod_exit();
}

void screen_aod_logic_init(void)
{
    esp_event_handler_instance_register(APP_EVENTS, EVT_MOTION_WRIST_SLEEP, wrist_sleep_handler, NULL, NULL);
    esp_event_handler_instance_register(APP_EVENTS, EVT_MOTION_WRIST_RAISE, wrist_raise_handler, NULL, NULL);
    ESP_LOGI(TAG, "AOD logic initialized");
}