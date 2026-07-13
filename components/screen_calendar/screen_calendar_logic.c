#include "screen_calendar_logic.h"
#include "ui.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "SCREEN_CALENDAR";

static void apply_theme(void)
{
    lv_obj_set_style_text_color(ui_Label4, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);

    // Сама сетка календаря (кнопки-дни) - LVGL9 lv_calendar рисует их как
    // button matrix, стилизуем через LV_PART_ITEMS/состояния.
    lv_obj_set_style_bg_color(ui_Calendar, lv_color_hex(UI_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_Calendar, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);

    lv_obj_set_style_bg_color(ui_Calendar, lv_color_hex(UI_COLOR_SURFACE), LV_PART_ITEMS);
    lv_obj_set_style_text_color(ui_Calendar, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_ITEMS);
    lv_obj_set_style_border_color(ui_Calendar, lv_color_hex(UI_COLOR_BORDER), LV_PART_ITEMS);
    lv_obj_set_style_border_width(ui_Calendar, 1, LV_PART_ITEMS);

    // Сегодняшний день - выделен акцентом
    lv_obj_set_style_bg_color(ui_Calendar, lv_color_hex(UI_COLOR_ACCENT), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ui_Calendar, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_CHECKED);

    // Дни соседних месяцев - приглушены
    lv_obj_set_style_text_color(ui_Calendar, lv_color_hex(UI_COLOR_TEXT_DISABLED), LV_PART_ITEMS | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(ui_Calendar, LV_OPA_50, LV_PART_ITEMS | LV_STATE_DISABLED);
}

void screen_calendar_logic_init(void)
{
    apply_theme();

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    lv_calendar_set_showed_date(ui_Calendar, tm_now.tm_year + 1900, tm_now.tm_mon + 1);
    lv_calendar_set_today_date(ui_Calendar, tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);

    ESP_LOGI(TAG, "Calendar screen logic initialized");
}

void ui_ScreenCalendar_full_init(void)
{
    ui_ScreenCalendar_screen_init();
    screen_calendar_logic_init();
}