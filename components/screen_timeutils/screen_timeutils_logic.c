#include "screen_timeutils_logic.h"
#include "ui.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

#include "svc_vibro.h"

static const char *TAG = "SCREEN_TIMEUTILS";

// =========================================================
// ============  TAB 1: ТАЙМЕР (3 вертикальных барабана)  ==
// =========================================================
static lv_obj_t *roller_h, *roller_m, *roller_s;
static lv_obj_t *timer_setup_row;
static lv_obj_t *timer_countdown_label;
static lv_obj_t *timer_btn_row;
static lv_obj_t *timer_start_btn, *timer_cancel_btn;

static lv_timer_t *timer_tick = NULL;
static int32_t timer_remaining_sec = 0;

static void build_roller_options(char *buf, size_t buf_sz, int max_exclusive)
{
    buf[0] = '\0';
    for (int i = 0; i < max_exclusive; i++) {
        char item[4];
        snprintf(item, sizeof(item), "%02d", i);
        strncat(buf, item, buf_sz - strlen(buf) - 1);
        if (i != max_exclusive - 1) strncat(buf, "\n", buf_sz - strlen(buf) - 1);
    }
}

static lv_obj_t *make_roller(lv_obj_t *parent, int max_exclusive)
{
    static char opts[300];
    build_roller_options(opts, sizeof(opts), max_exclusive);

    lv_obj_t *roller = lv_roller_create(parent);
    lv_roller_set_options(roller, opts, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_width(roller, 88);                                              // было 70

    // Без сплошной заливки — как системный picker на телефоне
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_30, LV_PART_MAIN);   // было дефолтный (14)
    lv_obj_set_style_text_color(roller, lv_color_hex(UI_COLOR_TEXT_DISABLED), LV_PART_MAIN);

    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_SELECTED);           // было UI_COLOR_ACCENT
    lv_obj_set_style_text_color(roller, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_30, LV_PART_SELECTED);

    return roller;
}

static void timer_tick_cb(lv_timer_t *t)
{
    (void)t;
    timer_remaining_sec--;
    if (timer_remaining_sec <= 0) {
        lv_label_set_text(timer_countdown_label, "00:00:00");
        lv_timer_del(timer_tick);
        timer_tick = NULL;
        svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_HIGH, DURATION_LONG);
        ESP_LOGI(TAG, "Timer finished");
        return;
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld",
             (long)(timer_remaining_sec / 3600),
             (long)((timer_remaining_sec / 60) % 60),
             (long)(timer_remaining_sec % 60));
    lv_label_set_text(timer_countdown_label, buf);
}

static void timer_show_setup(bool show)
{
    if (show) lv_obj_remove_flag(timer_setup_row, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(timer_setup_row, LV_OBJ_FLAG_HIDDEN);

    if (!show) lv_obj_remove_flag(timer_countdown_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(timer_countdown_label, LV_OBJ_FLAG_HIDDEN);
}

static void timer_start_cb(lv_event_t *e)
{
    (void)e;
    int h = lv_roller_get_selected(roller_h);
    int m = lv_roller_get_selected(roller_m);
    int s = lv_roller_get_selected(roller_s);
    timer_remaining_sec = h * 3600 + m * 60 + s;
    if (timer_remaining_sec <= 0) return;

    timer_show_setup(false);
    lv_obj_add_flag(timer_start_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(timer_cancel_btn, LV_OBJ_FLAG_HIDDEN);

    // Показываем стартовое значение сразу, без немедленного decrement
    char buf[12];
    snprintf(buf, sizeof(buf), "%02ld:%02ld:%02ld",
             (long)(timer_remaining_sec / 3600),
             (long)((timer_remaining_sec / 60) % 60),
             (long)(timer_remaining_sec % 60));
    lv_label_set_text(timer_countdown_label, buf);

    timer_tick = lv_timer_create(timer_tick_cb, 1000, NULL);
}

static void timer_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (timer_tick) { lv_timer_del(timer_tick); timer_tick = NULL; }
    timer_show_setup(true);
    lv_obj_remove_flag(timer_start_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(timer_cancel_btn, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *make_action_btn(lv_obj_t *parent, const char *text, uint32_t color, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 110, 46);
    lv_obj_set_style_radius(btn, 23, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    return btn;
}

static void build_timer_tab(void)
{
    UI_APPLY_SCREEN_BG(ui_TabPageTimer);

    timer_setup_row = lv_obj_create(ui_TabPageTimer);
    lv_obj_remove_style_all(timer_setup_row);
    lv_obj_set_size(timer_setup_row, lv_pct(90), 200);
    lv_obj_set_align(timer_setup_row, LV_ALIGN_CENTER);
    lv_obj_set_y(timer_setup_row, -40);
    lv_obj_set_flex_flow(timer_setup_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(timer_setup_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    roller_h = make_roller(timer_setup_row, 24);
    roller_m = make_roller(timer_setup_row, 60);
    roller_s = make_roller(timer_setup_row, 60);

    timer_countdown_label = lv_label_create(ui_TabPageTimer);
    lv_label_set_text(timer_countdown_label, "00:00:00");
    lv_obj_set_align(timer_countdown_label, LV_ALIGN_CENTER);
    lv_obj_set_y(timer_countdown_label, -40);
    lv_obj_set_style_text_color(timer_countdown_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(timer_countdown_label, &lv_font_montserrat_44, LV_PART_MAIN);
    lv_obj_add_flag(timer_countdown_label, LV_OBJ_FLAG_HIDDEN);

    timer_start_btn = make_action_btn(ui_TabPageTimer, "Start", UI_COLOR_ACCENT, timer_start_cb);
    lv_obj_set_align(timer_start_btn, LV_ALIGN_CENTER);
    lv_obj_set_y(timer_start_btn, 130);

    timer_cancel_btn = make_action_btn(ui_TabPageTimer, "Cancel", UI_COLOR_DANGER, timer_cancel_cb);
    lv_obj_set_align(timer_cancel_btn, LV_ALIGN_CENTER);
    lv_obj_set_y(timer_cancel_btn, 130);
    lv_obj_add_flag(timer_cancel_btn, LV_OBJ_FLAG_HIDDEN);
}

// =========================================================
// ============  TAB 2: СЕКУНДОМЕР  =========================
// =========================================================
typedef enum { SW_IDLE, SW_RUNNING, SW_PAUSED } stopwatch_state_t;

static stopwatch_state_t sw_state = SW_IDLE;
static int64_t sw_start_us = 0;      // esp_timer_get_time() в момент старта/резюма
static int64_t sw_accumulated_us = 0; // накопленное время до текущего старта/паузы

static lv_obj_t *sw_lbl_hhmm, *sw_lbl_ssms;
static lv_obj_t *sw_btn_left, *sw_btn_right; // левая/правая кнопка, меняют роль по состоянию
static lv_obj_t *sw_laps_list;
static lv_timer_t *sw_tick = NULL;
static int sw_lap_count = 0;

static int64_t sw_elapsed_us(void)
{
    if (sw_state == SW_RUNNING) {
        return sw_accumulated_us + (esp_timer_get_time() - sw_start_us);
    }
    return sw_accumulated_us;
}

static void sw_format(int64_t us, char *hhmm, size_t hhmm_sz, char *ssms, size_t ssms_sz)
{
    int64_t total_ms = us / 1000;
    int ms = (int)(total_ms % 1000);
    int64_t total_sec = total_ms / 1000;
    int sec = (int)(total_sec % 60);
    int64_t total_min = total_sec / 60;
    int min = (int)(total_min % 60);
    int hour = (int)(total_min / 60);
    snprintf(hhmm, hhmm_sz, "%02d:%02d", hour, min);
    snprintf(ssms, ssms_sz, "%02d.%03d", sec, ms);
}

static void sw_update_labels(void)
{
    char hhmm[8], ssms[8];
    sw_format(sw_elapsed_us(), hhmm, sizeof(hhmm), ssms, sizeof(ssms));
    lv_label_set_text(sw_lbl_hhmm, hhmm);
    lv_label_set_text(sw_lbl_ssms, ssms);
}

static void sw_tick_cb(lv_timer_t *t)
{
    (void)t;
    sw_update_labels();
}

static void sw_set_buttons_idle(void)
{
    lv_obj_add_flag(sw_btn_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(sw_btn_right, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_align(sw_btn_right, LV_ALIGN_CENTER);
    lv_obj_set_x(sw_btn_right, 0);
    lv_label_set_text(lv_obj_get_child(sw_btn_right, 0), "Start");
    lv_obj_set_style_bg_color(sw_btn_right, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);
}

static void sw_set_buttons_running(void)
{
    lv_obj_remove_flag(sw_btn_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(sw_btn_right, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_align(sw_btn_left, LV_ALIGN_CENTER);
    lv_obj_set_x(sw_btn_left, -65);
    lv_obj_set_align(sw_btn_right, LV_ALIGN_CENTER);
    lv_obj_set_x(sw_btn_right, 65);
    lv_label_set_text(lv_obj_get_child(sw_btn_left, 0), "Lap");
    lv_label_set_text(lv_obj_get_child(sw_btn_right, 0), "Pause");
    lv_obj_set_style_bg_color(sw_btn_left, lv_color_hex(UI_COLOR_SURFACE_ALT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw_btn_right, lv_color_hex(UI_COLOR_DANGER), LV_PART_MAIN);
}

static void sw_set_buttons_paused(void)
{
    lv_obj_remove_flag(sw_btn_left, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(sw_btn_right, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(sw_btn_left, 0), "Reset");
    lv_label_set_text(lv_obj_get_child(sw_btn_right, 0), "Resume");
    lv_obj_set_style_bg_color(sw_btn_left, lv_color_hex(UI_COLOR_SURFACE_ALT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw_btn_right, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);
}

static void sw_add_lap_row(int index, int64_t us)
{
    char hhmm[8], ssms[8], buf[40];
    sw_format(us, hhmm, sizeof(hhmm), ssms, sizeof(ssms));
    snprintf(buf, sizeof(buf), "Lap %d   %s:%s", index, hhmm, ssms);

    lv_obj_t *lbl = lv_label_create(sw_laps_list);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
}

static void sw_right_btn_cb(lv_event_t *e)
{
    (void)e;
    switch (sw_state) {
    case SW_IDLE:
        if (sw_tick) { lv_timer_del(sw_tick); sw_tick = NULL; }   // ← защита
        sw_state = SW_RUNNING;
        sw_start_us = esp_timer_get_time();
        sw_accumulated_us = 0;
        sw_lap_count = 0;
        lv_obj_clean(sw_laps_list);
        sw_tick = lv_timer_create(sw_tick_cb, 50, NULL);
        sw_set_buttons_running();
        break;
    case SW_RUNNING: // Pause
        sw_accumulated_us = sw_elapsed_us();
        sw_state = SW_PAUSED;
        if (sw_tick) { lv_timer_del(sw_tick); sw_tick = NULL; }
        sw_update_labels();
        sw_set_buttons_paused();
        break;
    case SW_PAUSED: // Resume
        if (sw_tick) { lv_timer_del(sw_tick); sw_tick = NULL; }   // ← защита
        sw_state = SW_RUNNING;
        sw_start_us = esp_timer_get_time();
        sw_tick = lv_timer_create(sw_tick_cb, 50, NULL);
        sw_set_buttons_running();
        break;
    }
}

static void sw_left_btn_cb(lv_event_t *e)
{
    (void)e;
    if (sw_state == SW_RUNNING) {
        sw_lap_count++;
        sw_add_lap_row(sw_lap_count, sw_elapsed_us());
    } else if (sw_state == SW_PAUSED) {
        // Reset
        sw_state = SW_IDLE;
        sw_accumulated_us = 0;
        sw_lap_count = 0;
        lv_obj_clean(sw_laps_list);
        sw_update_labels();
        sw_set_buttons_idle();
    }
}

static void build_stopwatch_tab(void)
{
    UI_APPLY_SCREEN_BG(ui_TabPageStopwatch);

    sw_lbl_hhmm = lv_label_create(ui_TabPageStopwatch);
    lv_label_set_text(sw_lbl_hhmm, "00:00");
    lv_obj_set_align(sw_lbl_hhmm, LV_ALIGN_TOP_MID);
    lv_obj_set_y(sw_lbl_hhmm, 40);
    lv_obj_set_style_text_color(sw_lbl_hhmm, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(sw_lbl_hhmm, &lv_font_montserrat_48, LV_PART_MAIN);   // было _44

    sw_lbl_ssms = lv_label_create(ui_TabPageStopwatch);
    lv_label_set_text(sw_lbl_ssms, "00.000");
    lv_obj_set_align(sw_lbl_ssms, LV_ALIGN_TOP_MID);
    lv_obj_set_y(sw_lbl_ssms, 98);                                                   // было 82
    lv_obj_set_style_text_color(sw_lbl_ssms, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_font(sw_lbl_ssms, &lv_font_montserrat_44, LV_PART_MAIN);   // было _30

    sw_btn_left = make_action_btn(ui_TabPageStopwatch, "Lap", UI_COLOR_SURFACE_ALT, sw_left_btn_cb);
    lv_obj_set_align(sw_btn_left, LV_ALIGN_TOP_MID);
    lv_obj_set_y(sw_btn_left, 170);                                                  // было 150
    lv_obj_set_x(sw_btn_left, -65);

    sw_btn_right = make_action_btn(ui_TabPageStopwatch, "Start", UI_COLOR_ACCENT, sw_right_btn_cb);
    lv_obj_set_align(sw_btn_right, LV_ALIGN_TOP_MID);
    lv_obj_set_y(sw_btn_right, 170);                                                 // было 150

    sw_laps_list = lv_obj_create(ui_TabPageStopwatch);
    lv_obj_remove_style_all(sw_laps_list);
    lv_obj_set_size(sw_laps_list, lv_pct(90), 130);                                  // было 150
    lv_obj_set_align(sw_laps_list, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(sw_laps_list, -10);
    lv_obj_set_flex_flow(sw_laps_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(sw_laps_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(sw_laps_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(sw_laps_list, LV_DIR_VER);

    sw_set_buttons_idle();
}

static void logic_init(void)
{
    build_timer_tab();
    build_stopwatch_tab();
    ESP_LOGI(TAG, "TimeUtils screen logic initialized");
}

static void logic_deinit(void)
{
    if (timer_tick) { lv_timer_delete(timer_tick); timer_tick = NULL; }
    if (sw_tick)    { lv_timer_delete(sw_tick);    sw_tick = NULL; }

    // Состояние живёт ровно столько же, сколько экран: уходим - таймер и
    // секундомер сбрасываются (иначе виджеты после пересоздания не совпали бы с ним).
    timer_remaining_sec = 0;
    sw_state = SW_IDLE;
    sw_start_us = 0;
    sw_accumulated_us = 0;
    sw_lap_count = 0;

    roller_h = roller_m = roller_s = NULL;
    timer_setup_row = timer_countdown_label = timer_btn_row = NULL;
    timer_start_btn = timer_cancel_btn = NULL;
    sw_lbl_hhmm = sw_lbl_ssms = NULL;
    sw_btn_left = sw_btn_right = NULL;
    sw_laps_list = NULL;
}

lv_obj_t *screen_timeutils_create(void)
{
    ui_ScreenTimeUtils_screen_init();
    logic_init();
    return ui_ScreenTimeUtils;
}

void screen_timeutils_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenTimeUtils
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenTimeUtils_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
