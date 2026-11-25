#include "screen_aod_logic.h"
#include "ui.h"
#include "app_nav.h"
#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include <time.h>
#include <math.h>

static const char *TAG = "SCREEN_AOD";

// Центр экрана — ЕДИНАЯ точка отсчёта
#define CX  (EXAMPLE_LCD_H_RES / 2)
#define CY  (EXAMPLE_LCD_V_RES / 2)

static lv_timer_t *s_tick_timer = NULL;
static bool s_in_aod = false;                       // "хотим быть в AOD" (учитывает отложенные переходы nav)
static app_evt_sub_t s_sub_sleep, s_sub_raise;      // триггеры входа/выхода, живут весь uptime

static void aod_update_hands(void)
{
    if (ui_AodHourHand == NULL) return;             // экрана сейчас нет

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

// ---------------------------------------------------------
// Жизненный цикл экрана (для app_nav)
// ---------------------------------------------------------
lv_obj_t *screen_aod_create(void)
{
    ui_ScreenAOD_screen_init();
    aod_update_hands();
    // Таймер существует только пока существует экран: в остальное время AOD не тратит CPU.
    s_tick_timer = lv_timer_create(aod_tick_timer_cb, 1000, NULL);
    return ui_ScreenAOD;
}

void screen_aod_destroy(lv_obj_t *scr)
{
    (void)scr;
    if (s_tick_timer) { lv_timer_delete(s_tick_timer); s_tick_timer = NULL; }
    ui_ScreenAOD_screen_destroy();                  // lv_obj_del + обнуление ui_Aod*
}

// ---------------------------------------------------------
// Вход / выход: AOD - оверлей поверх стека навигатора.
// push оставляет текущий экран жить (его вернёт pop), так что таймер
// обратного отсчёта, SD-путь и т.п. не теряются, пока часы "спят".
// ---------------------------------------------------------
void screen_aod_enter(void)
{
    if (s_in_aod) return;
    s_in_aod = true;
    nav_push_ex("aod", LV_SCR_LOAD_ANIM_FADE_ON, 200);
    ESP_LOGI(TAG, "Entered AOD watchface");
}

void screen_aod_exit(void)
{
    if (!s_in_aod) return;
    s_in_aod = false;
    nav_pop_ex(LV_SCR_LOAD_ANIM_FADE_ON, 200);
    ESP_LOGI(TAG, "Exited AOD watchface");
}

void screen_aod_exit_to_home(void)
{
    if (!s_in_aod) return;
    s_in_aod = false;
    nav_back_to_home_ex(LV_SCR_LOAD_ANIM_FADE_ON, 200);
    ESP_LOGI(TAG, "Exited AOD watchface -> home");
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

// Постоянные триггеры (не принадлежат экрану, поэтому destroy() их не снимает).
void screen_aod_logic_init(void)
{
    app_evt_subscribe(&s_sub_sleep, EVT_MOTION_WRIST_SLEEP, wrist_sleep_handler, NULL);
    app_evt_subscribe(&s_sub_raise, EVT_MOTION_WRIST_RAISE, wrist_raise_handler, NULL);
    ESP_LOGI(TAG, "AOD triggers armed");
}
