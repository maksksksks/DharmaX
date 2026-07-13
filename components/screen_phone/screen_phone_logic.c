#include "screen_phone_logic.h"
#include "ui.h"
#include "app_events.h"
#include "svc_ble_sync.h"
#include "esp_event.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "SCREEN_PHONE";

static lv_obj_t *lbl_title;
static lv_obj_t *lbl_artist;
static lv_obj_t *btn_prev, *btn_play, *btn_next;
static lv_obj_t *lbl_play_icon;
static lv_obj_t *bar_progress;
static lv_obj_t *lbl_time_elapsed, *lbl_time_total;

static bool s_is_playing = false;

static void format_mmss(uint32_t seconds, char *buf, size_t buf_sz)
{
    snprintf(buf, buf_sz, "%lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
}

static void btn_prev_cb(lv_event_t *e)
{
    (void)e;
    send_action_to_phone(ACTION_MEDIA_PREV);
}

static void btn_next_cb(lv_event_t *e)
{
    (void)e;
    send_action_to_phone(ACTION_MEDIA_NEXT);
}

static void btn_play_cb(lv_event_t *e)
{
    (void)e;
    send_action_to_phone(ACTION_MEDIA_PLAY_PAUSE);
    s_is_playing = !s_is_playing;
    lv_label_set_text(lbl_play_icon, s_is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

static lv_obj_t *make_round_btn(lv_obj_t *parent, const char *symbol, int diameter, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, diameter, diameter);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN);

    return btn;
}

static void build_player_ui(void)
{
    lv_obj_add_flag(ui_Label9, LV_OBJ_FLAG_HIDDEN);
    UI_APPLY_SCREEN_BG(ui_ScreenPhone);

    lbl_title = lv_label_create(ui_ScreenPhone);
    lv_label_set_text(lbl_title, "No media");
    lv_obj_set_width(lbl_title, lv_pct(85));
    lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_align(lbl_title, LV_ALIGN_CENTER);
    lv_obj_set_y(lbl_title, -110);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, LV_PART_MAIN);

    lbl_artist = lv_label_create(ui_ScreenPhone);
    lv_label_set_text(lbl_artist, "-");
    lv_obj_set_width(lbl_artist, lv_pct(85));
    lv_label_set_long_mode(lbl_artist, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl_artist, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_align(lbl_artist, LV_ALIGN_CENTER);
    lv_obj_set_y(lbl_artist, -82);
    lv_obj_set_style_text_color(lbl_artist, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_artist, &lv_font_montserrat_14, LV_PART_MAIN);

    btn_play = make_round_btn(ui_ScreenPhone, LV_SYMBOL_PLAY, 64, btn_play_cb);
    lv_obj_set_align(btn_play, LV_ALIGN_CENTER);
    lv_obj_set_y(btn_play, -10);
    lbl_play_icon = lv_obj_get_child(btn_play, 0);

    btn_prev = make_round_btn(ui_ScreenPhone, LV_SYMBOL_PREV, 50, btn_prev_cb);
    lv_obj_set_align(btn_prev, LV_ALIGN_CENTER);
    lv_obj_set_x(btn_prev, -84);
    lv_obj_set_y(btn_prev, -10);

    btn_next = make_round_btn(ui_ScreenPhone, LV_SYMBOL_NEXT, 50, btn_next_cb);
    lv_obj_set_align(btn_next, LV_ALIGN_CENTER);
    lv_obj_set_x(btn_next, 84);
    lv_obj_set_y(btn_next, -10);

    bar_progress = lv_bar_create(ui_ScreenPhone);
    lv_obj_set_size(bar_progress, lv_pct(78), 6);
    lv_obj_set_align(bar_progress, LV_ALIGN_CENTER);
    lv_obj_set_y(bar_progress, 60);
    lv_bar_set_range(bar_progress, 0, 1000);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_progress, lv_color_hex(UI_COLOR_SURFACE_ALT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_progress, lv_color_hex(UI_COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_progress, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_progress, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    lbl_time_elapsed = lv_label_create(ui_ScreenPhone);
    lv_label_set_text(lbl_time_elapsed, "0:00");
    lv_obj_align_to(lbl_time_elapsed, bar_progress, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_set_style_text_color(lbl_time_elapsed, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_time_elapsed, &lv_font_montserrat_14, LV_PART_MAIN);

    lbl_time_total = lv_label_create(ui_ScreenPhone);
    lv_label_set_text(lbl_time_total, "0:00");
    lv_obj_align_to(lbl_time_total, bar_progress, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 4);
    lv_obj_set_style_text_color(lbl_time_total, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_time_total, &lv_font_montserrat_14, LV_PART_MAIN);
}

static void media_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id;
    media_data_t *m = (media_data_t *)event_data;

    lv_label_set_text(lbl_title, m->title[0] ? m->title : "No media");
    lv_label_set_text(lbl_artist, m->artist[0] ? m->artist : "-");

    s_is_playing = m->is_playing;
    lv_label_set_text(lbl_play_icon, s_is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    char buf[8];
    format_mmss(m->position_sec, buf, sizeof(buf));
    lv_label_set_text(lbl_time_elapsed, buf);
    format_mmss(m->duration_sec, buf, sizeof(buf));
    lv_label_set_text(lbl_time_total, buf);

    int32_t progress = 0;
    if (m->duration_sec > 0) {
        progress = (int32_t)((uint64_t)m->position_sec * 1000 / m->duration_sec);
    }
    lv_bar_set_value(bar_progress, progress, LV_ANIM_ON);
}

void screen_phone_logic_init(void)
{
    build_player_ui();
    esp_event_handler_instance_register(APP_EVENTS, EVT_BLE_MEDIA_UPDATED, media_event_handler, NULL, NULL);
    ESP_LOGI(TAG, "Phone (player) screen logic initialized");
}

void ui_ScreenPhone_full_init(void)
{
    ui_ScreenPhone_screen_init();
    screen_phone_logic_init();
}