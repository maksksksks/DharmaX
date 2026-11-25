#include "ui.h"
#include "app_nav.h"
#include "screen_aod_logic.h"
#include "svc_screen_sleep.h"
#include <math.h>

lv_obj_t * ui_ScreenAOD = NULL;
lv_obj_t * ui_AodRing = NULL;
lv_obj_t * ui_AodHourHand = NULL;
lv_obj_t * ui_AodMinuteHand = NULL;
lv_obj_t * ui_AodSecondDot = NULL;

// =========================================================
// ГЕОМЕТРИЯ КОЛЬЦА
// =========================================================
#define AOD_RING_RADIUS       120
#define AOD_RING_WAVE_AMP     6
#define AOD_RING_BUMPS        12
#define AOD_RING_POINTS       360
#define AOD_RING_WIDTH        4

// ★ Фазовый сдвиг: горб волны на позиции "1 час" (30°)
#define AOD_RING_PHASE        ((float)M_PI / 6.0f)   // 30° в радианах

// Offset для положительных координат
#define AOD_RING_OFFSET  (AOD_RING_RADIUS + AOD_RING_WAVE_AMP + AOD_RING_WIDTH)

// =========================================================
// ЦВЕТА — БЕЗ swap (панель RGB, остальные цвета в проекте работают)
// =========================================================
#define AOD_COLOR_HOUR        0xD0BCFF  // rgb(208, 188, 255)
#define AOD_COLOR_MINUTE      0x9F91BF  // rgb(159, 145, 191)
#define AOD_COLOR_RING        0x4A3F6B  // тёмный лавандовый
#define AOD_COLOR_SECOND      0xD0BCFF  // = часовая

// =========================================================
// ЦЕНТР ЭКРАНА (единая точка отсчёта для ВСЕХ объектов)
// =========================================================
#define CX  (EXAMPLE_LCD_H_RES / 2)   // 140
#define CY  (EXAMPLE_LCD_V_RES / 2)   // 228

static lv_point_precise_t ring_points[AOD_RING_POINTS + 1];

void ui_event_ScreenAOD(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_CLICKED || event_code == LV_EVENT_GESTURE) {
        if (event_code == LV_EVENT_GESTURE) {
            lv_indev_wait_release(lv_indev_active());
        }
        // Касание по AOD всегда ведёт на home (как и раньше): сначала уходим
        // из AOD на home, потом будим экран (wake_up видит, что AOD уже закрыт).
        screen_aod_exit_to_home();
        svc_screen_sleep_wake_up();
    }
}

// =========================================================
// КОЛЬЦО: горб на "1 час", все точки ≥ 0
// =========================================================
static void build_ring_points(void)
{
    for (int i = 0; i <= AOD_RING_POINTS; i++) {
        float theta = (2.0f * (float)M_PI) * (float)i / (float)AOD_RING_POINTS;
        // ★ cos((theta - PHASE) * BUMPS): максимум при theta = PHASE = 30° (1 час)
        float wave = AOD_RING_WAVE_AMP * cosf((theta - AOD_RING_PHASE) * AOD_RING_BUMPS);
        float r = AOD_RING_RADIUS + wave;
        ring_points[i].x = (lv_value_precise_t)(r * sinf(theta) + AOD_RING_OFFSET);
        ring_points[i].y = (lv_value_precise_t)(-r * cosf(theta) + AOD_RING_OFFSET);
    }
}

// =========================================================
// СТРЕЛКА: позиционирование через lv_obj_set_pos (абсолютные координаты)
// Pivot на (width/2, len - width/2) → нахлёст за центр на РАДИУС стрелки
// =========================================================
static lv_obj_t * create_hand(lv_obj_t * parent, int w, int len, uint32_t color)
{
    lv_obj_t * hand = lv_obj_create(parent);
    lv_obj_remove_style_all(hand);
    lv_obj_set_size(hand, w, len);

    // ★ Абсолютная позиция: pivot должен быть в (CX, CY)
    // pivot_local = (w/2, len - w/2)
    // top_left = pivot_screen - pivot_local = (CX - w/2, CY - len + w/2)
    lv_obj_set_pos(hand, CX - w / 2, CY - len + w / 2);

    lv_obj_set_style_radius(hand, w / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hand, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hand, LV_OPA_COVER, LV_PART_MAIN);

    // ★ Pivot: нахлёст на РАДИУС (w/2) от нижнего края
    lv_obj_set_style_transform_pivot_x(hand, w / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(hand, len - w / 2, LV_PART_MAIN);

    lv_obj_remove_flag(hand, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return hand;
}

void ui_ScreenAOD_screen_init(void)
{
    ui_ScreenAOD = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_ScreenAOD, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_ScreenAOD, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenAOD, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ScreenAOD, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(ui_ScreenAOD, 0, LV_PART_MAIN);

    // ─── Кольцо ───
    build_ring_points();
    ui_AodRing = lv_line_create(ui_ScreenAOD);
    lv_obj_remove_style_all(ui_AodRing);
    lv_line_set_points(ui_AodRing, ring_points, AOD_RING_POINTS + 1);
    lv_obj_set_style_line_width(ui_AodRing, AOD_RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_color(ui_AodRing, lv_color_hex(AOD_COLOR_RING), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(ui_AodRing, true, LV_PART_MAIN);
    lv_obj_set_style_line_opa(ui_AodRing, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui_AodRing, 0, LV_PART_MAIN);
    lv_obj_remove_flag(ui_AodRing, LV_OBJ_FLAG_CLICKABLE);
    // ★ Позиция: центр кольца (OFFSET, OFFSET) → в (CX, CY)
    lv_obj_set_pos(ui_AodRing, CX - AOD_RING_OFFSET, CY - AOD_RING_OFFSET);

    // ─── Стрелки (pivot в CX, CY) ───
    ui_AodMinuteHand = create_hand(ui_ScreenAOD, AOD_HAND_WIDTH, AOD_MINUTE_HAND_LEN, AOD_COLOR_MINUTE);
    ui_AodHourHand   = create_hand(ui_ScreenAOD, AOD_HAND_WIDTH, AOD_HOUR_HAND_LEN,   AOD_COLOR_HOUR);

    // ─── Секундный кружок (начальная позиция — 12 часов) ───
    ui_AodSecondDot = lv_obj_create(ui_ScreenAOD);
    lv_obj_remove_style_all(ui_AodSecondDot);
    lv_obj_set_size(ui_AodSecondDot, AOD_SECOND_DOT_SIZE, AOD_SECOND_DOT_SIZE);
    lv_obj_set_style_radius(ui_AodSecondDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_AodSecondDot, lv_color_hex(AOD_COLOR_SECOND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_AodSecondDot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(ui_AodSecondDot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    // Начальная позиция (12 часов): центр кружка в (CX, CY - TRACK_R)
    lv_obj_set_pos(ui_AodSecondDot,
                   CX - AOD_SECOND_DOT_SIZE / 2,
                   CY - AOD_SECOND_TRACK_R - AOD_SECOND_DOT_SIZE / 2);

    lv_obj_add_event_cb(ui_ScreenAOD, ui_event_ScreenAOD, LV_EVENT_ALL, NULL);
}

void ui_ScreenAOD_screen_destroy(void)
{
    if (ui_ScreenAOD) lv_obj_del(ui_ScreenAOD);
    ui_ScreenAOD = NULL;
    ui_AodRing = NULL;
    ui_AodHourHand = NULL;
    ui_AodMinuteHand = NULL;
    ui_AodSecondDot = NULL;
}