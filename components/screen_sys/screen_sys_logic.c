#include "screen_sys_logic.h"
#include "ui.h"
#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "SCREEN_SYS";

static lv_obj_t *lbl_signal;
static lv_obj_t *lbl_battery;
static lv_obj_t *lbl_carrier;
static lv_obj_t *lbl_connection_state;

static void build_ui(void)
{
    UI_APPLY_SCREEN_BG(ui_ScreenSys);
    lv_obj_set_style_text_color(ui_Label17, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);

    // "Phone system" из SquareLine поднимаем повыше и используем как заголовок
    lv_obj_set_y(ui_Label17, -170);

    lbl_connection_state = lv_label_create(ui_ScreenSys);
    lv_label_set_text(lbl_connection_state, LV_SYMBOL_CLOSE " Not connected");
    lv_obj_set_align(lbl_connection_state, LV_ALIGN_CENTER);
    lv_obj_set_y(lbl_connection_state, -100);
    lv_obj_set_style_text_color(lbl_connection_state, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_connection_state, &lv_font_montserrat_14, LV_PART_MAIN);

    // Карточка со статусом (та же стилистика, что card_feed, но статичная)
    lv_obj_t *card = lv_obj_create(ui_ScreenSys);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, lv_pct(80), 140);
    lv_obj_set_align(card, LV_ALIGN_CENTER);
    lv_obj_set_y(card, 20);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(UI_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lbl_signal = lv_label_create(card);
    lv_label_set_text(lbl_signal, LV_SYMBOL_WIFI " Signal: --");
    lv_obj_set_style_text_color(lbl_signal, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_signal, &lv_font_montserrat_14, LV_PART_MAIN);

    lbl_battery = lv_label_create(card);
    lv_label_set_text(lbl_battery, LV_SYMBOL_BATTERY_FULL " Battery: --");
    lv_obj_set_style_text_color(lbl_battery, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_battery, &lv_font_montserrat_14, LV_PART_MAIN);

    lbl_carrier = lv_label_create(card);
    lv_label_set_text(lbl_carrier, LV_SYMBOL_CALL " Carrier: --");
    lv_obj_set_style_text_color(lbl_carrier, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_carrier, &lv_font_montserrat_14, LV_PART_MAIN);
}

static void phone_status_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id;
    phone_status_data_t *ps = (phone_status_data_t *)event_data;

    lv_label_set_text(lbl_connection_state, LV_SYMBOL_OK " Connected");
    lv_obj_set_style_text_color(lbl_connection_state, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);

    char buf[32];
    if (ps->signal_percent >= 0) snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " Signal: %d%%", ps->signal_percent);
    else snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " Signal: --");
    lv_label_set_text(lbl_signal, buf);

    if (ps->battery_percent >= 0) {
        snprintf(buf, sizeof(buf), "%s Battery: %d%%%s",
                 ps->charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_FULL,
                 ps->battery_percent, ps->charging ? " (charging)" : "");
    } else {
        snprintf(buf, sizeof(buf), LV_SYMBOL_BATTERY_FULL " Battery: --");
    }
    lv_label_set_text(lbl_battery, buf);

    snprintf(buf, sizeof(buf), LV_SYMBOL_CALL " Carrier: %s", ps->carrier[0] ? ps->carrier : "--");
    lv_label_set_text(lbl_carrier, buf);
}

void screen_sys_logic_init(void)
{
    build_ui();
    esp_event_handler_instance_register(APP_EVENTS, EVT_BLE_PHONE_STATUS_UPDATED, phone_status_handler, NULL, NULL);
    ESP_LOGI(TAG, "Sys (phone status) screen logic initialized");
}

void ui_ScreenSys_full_init(void)
{
    ui_ScreenSys_screen_init();
    screen_sys_logic_init();
}