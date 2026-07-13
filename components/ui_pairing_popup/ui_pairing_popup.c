#include "ui_pairing_popup.h"
#include "ui.h"

static bool s_popup_open = false;
static lv_obj_t *s_popup_bg = NULL;

void close_pairing_popup_cb(lv_event_t *e)
{
    (void)e;
    if (s_popup_bg != NULL) {
        s_popup_open = false;
        lv_obj_delete(s_popup_bg);
        s_popup_bg = NULL;
    }
}

void show_pairing_popup(const char *code)
{
    s_popup_open = true;
    if (s_popup_bg != NULL) return; // уже открыт

    // Затемняющий фон поверх активного экрана
    s_popup_bg = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_popup_bg);
    lv_obj_set_size(s_popup_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_popup_bg, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_popup_bg, LV_OPA_70, 0);
    lv_obj_add_flag(s_popup_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(s_popup_bg);

    // Карточка с кодом
    lv_obj_t *card = lv_obj_create(s_popup_bg);
    lv_obj_set_size(card, 220, 170);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, 255, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(UI_COLOR_ACCENT), 0);

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "Pairing code");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    lv_obj_t *code_label = lv_label_create(card);
    lv_label_set_text(code_label, code);
    lv_obj_set_style_text_font(code_label, &lv_font_montserrat_30, 0);
    lv_obj_set_style_text_color(code_label, lv_color_hex(UI_COLOR_ACCENT), 0);
    lv_obj_center(code_label);

    lv_obj_t *close_btn = lv_button_create(card);
    lv_obj_set_size(close_btn, 90, 38);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(UI_COLOR_SURFACE_ALT), 0);
    lv_obj_add_event_cb(close_btn, close_pairing_popup_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(close_btn);
    lv_label_set_text(btn_label, "Close");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_center(btn_label);
}

bool get_popup_status(void)
{
    return s_popup_open;
}
