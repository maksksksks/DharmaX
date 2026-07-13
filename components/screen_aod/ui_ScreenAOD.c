// Hand-written screen (не из SquareLine Studio): AOD-циферблат для AMOLED.
// Волнистая линия огибает циферблат по кругу, волны там, где были бы цифры
// (12 "горбов" на 12 часовых позиций), самих цифр нет.
// Часовая и минутная стрелки - линии с закруглёнными концами (часовая короче
// и ярче). Секундная стрелка - кружок того же диаметра, что и толщина
// стрелок, цветом как часовая, бегущий по внешней окружности.

#include "ui.h"
#include <math.h>

lv_obj_t * ui_ScreenAOD = NULL;
lv_obj_t * ui_AodRing = NULL;
lv_obj_t * ui_AodHourHand = NULL;
lv_obj_t * ui_AodMinuteHand = NULL;
lv_obj_t * ui_AodSecondDot = NULL;

// =========================================================
// ГЕОМЕТРИЯ ЦИФЕРБЛАТА
// Подгоните AOD_FACE_RADIUS под реальное разрешение вашего AMOLED-экрана.
// =========================================================
#define AOD_FACE_RADIUS       130   // радиус лицевой стороны часов, px
#define AOD_RING_RADIUS       118   // базовый радиус волнистой линии
#define AOD_RING_WAVE_AMP     7     // амплитуда волны, px
#define AOD_RING_BUMPS        12    // число "горбов" волны = число часовых меток
#define AOD_RING_POINTS       240   // точек на всю окружность (сглаженность)
#define AOD_RING_WIDTH        2     // толщина волнистой линии
// AOD_HAND_WIDTH / AOD_HOUR_HAND_LEN / AOD_MINUTE_HAND_LEN / AOD_SECOND_TRACK_R
// объявлены в ui_ScreenAOD.h - они нужны и здесь, и в screen_aod_logic.c

#define AOD_COLOR_RING      UI_COLOR_BORDER    // тусклый, чтобы не жечь лишние субпиксели AMOLED
#define AOD_COLOR_MINUTE    UI_COLOR_ACCENT_DIM // минутная - приглушённый акцент
#define AOD_COLOR_HOUR      UI_COLOR_ACCENT     // часовая - полный акцент, ярче
#define AOD_COLOR_SECOND    UI_COLOR_ACCENT     // точка секунд = цвет часовой стрелки

static lv_point_precise_t ring_points[AOD_RING_POINTS + 1];

// event funtions
void ui_event_ScreenAOD(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    // Любое касание экрана в режиме AOD будит часы и возвращает на Home.
    if (event_code == LV_EVENT_CLICKED || event_code == LV_EVENT_GESTURE) {
        if (event_code == LV_EVENT_GESTURE) {
            lv_indev_wait_release(lv_indev_active());
        }
        extern void svc_screen_sleep_wake_up(void);
        svc_screen_sleep_wake_up();
        _ui_screen_change(&ui_ScreenHome, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, &ui_ScreenHome_screen_init);
    }
}

// build funtions

static void build_ring_points(void)
{
    for (int i = 0; i <= AOD_RING_POINTS; i++) {
        float theta = (2.0f * (float)M_PI) * (float)i / (float)AOD_RING_POINTS;
        float wave = AOD_RING_WAVE_AMP * sinf(theta * AOD_RING_BUMPS);
        float r = AOD_RING_RADIUS + wave;
        ring_points[i].x = (lv_value_precise_t)(r * sinf(theta));
        ring_points[i].y = (lv_value_precise_t)(-r * cosf(theta));
    }
}

void ui_ScreenAOD_screen_init(void)
{
    ui_ScreenAOD = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_ScreenAOD, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_ScreenAOD, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenAOD, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_ScreenAOD, LV_OBJ_FLAG_CLICKABLE);

    // --- Волнистое кольцо вместо цифр ---
    build_ring_points();
    ui_AodRing = lv_line_create(ui_ScreenAOD);
    lv_line_set_points(ui_AodRing, ring_points, AOD_RING_POINTS + 1);
    lv_obj_set_align(ui_AodRing, LV_ALIGN_CENTER);
    lv_obj_set_style_line_width(ui_AodRing, AOD_RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_line_color(ui_AodRing, lv_color_hex(AOD_COLOR_RING), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(ui_AodRing, true, LV_PART_MAIN);
    lv_obj_set_style_line_opa(ui_AodRing, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(ui_AodRing, LV_OBJ_FLAG_CLICKABLE);

    // --- Минутная стрелка (длиннее, тусклее) ---
    ui_AodMinuteHand = lv_obj_create(ui_ScreenAOD);
    lv_obj_remove_style_all(ui_AodMinuteHand);
    lv_obj_set_size(ui_AodMinuteHand, AOD_HAND_WIDTH, AOD_MINUTE_HAND_LEN);
    lv_obj_set_align(ui_AodMinuteHand, LV_ALIGN_CENTER);
    lv_obj_set_y(ui_AodMinuteHand, -AOD_MINUTE_HAND_LEN / 2);
    lv_obj_set_style_radius(ui_AodMinuteHand, AOD_HAND_WIDTH / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_AodMinuteHand, lv_color_hex(AOD_COLOR_MINUTE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_AodMinuteHand, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(ui_AodMinuteHand, AOD_HAND_WIDTH / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(ui_AodMinuteHand, AOD_MINUTE_HAND_LEN, LV_PART_MAIN);
    lv_obj_remove_flag(ui_AodMinuteHand, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    // --- Часовая стрелка (короче, ярче) ---
    ui_AodHourHand = lv_obj_create(ui_ScreenAOD);
    lv_obj_remove_style_all(ui_AodHourHand);
    lv_obj_set_size(ui_AodHourHand, AOD_HAND_WIDTH, AOD_HOUR_HAND_LEN);
    lv_obj_set_align(ui_AodHourHand, LV_ALIGN_CENTER);
    lv_obj_set_y(ui_AodHourHand, -AOD_HOUR_HAND_LEN / 2);
    lv_obj_set_style_radius(ui_AodHourHand, AOD_HAND_WIDTH / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_AodHourHand, lv_color_hex(AOD_COLOR_HOUR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_AodHourHand, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(ui_AodHourHand, AOD_HAND_WIDTH / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(ui_AodHourHand, AOD_HOUR_HAND_LEN, LV_PART_MAIN);
    lv_obj_remove_flag(ui_AodHourHand, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    // --- Секундная стрелка = кружок, диаметр = толщина стрелок, цвет как у часовой ---
    ui_AodSecondDot = lv_obj_create(ui_ScreenAOD);
    lv_obj_remove_style_all(ui_AodSecondDot);
    lv_obj_set_size(ui_AodSecondDot, AOD_HAND_WIDTH, AOD_HAND_WIDTH);
    lv_obj_set_style_radius(ui_AodSecondDot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_AodSecondDot, lv_color_hex(AOD_COLOR_SECOND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_AodSecondDot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_align(ui_AodSecondDot, LV_ALIGN_CENTER);
    lv_obj_remove_flag(ui_AodSecondDot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

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
