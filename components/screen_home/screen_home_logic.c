#include "screen_home_logic.h"
#include "ui.h"
#include "app_events.h"
#include "esp_event.h"
#include "esp_log.h"
#include "svc_screen_sleep.h"
#include <time.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "SCREEN_HOME";

// --- Виджет шагомера, который мы достраиваем внутри ui_Panel1 ---
static lv_obj_t *ui_HomeArcSteps;
static lv_obj_t *ui_HomeLabelSteps;

#define STEPS_GOAL 10000

// =========================================================
// ДАТА / ВРЕМЯ (перенесено из старого home_tile.c)
// =========================================================
static const char *get_month_ru(int num)
{
    switch (num) {
    case 1: return "Января";
    case 2: return "Февраля";
    case 3: return "Марта";
    case 4: return "Апреля";
    case 5: return "Мая";
    case 6: return "Июня";
    case 7: return "Июля";
    case 8: return "Августа";
    case 9: return "Сентября";
    case 10: return "Октября";
    case 11: return "Ноября";
    case 12: return "Декабря";
    default: return "?";
    }
}

static const char *get_weekday_ru(int wday) // tm_wday: 0=Sunday
{
    switch (wday) {
    case 0: return "Воскресенье";
    case 1: return "Понедельник";
    case 2: return "Вторник";
    case 3: return "Среда";
    case 4: return "Четверг";
    case 5: return "Пятница";
    case 6: return "Суббота";
    default: return "?";
    }
}

static void update_clock_labels(void)
{
    time_t now;
    struct tm tm_now;
    time(&now);
    localtime_r(&now, &tm_now);

    char hh[3], mm[3], ss[3], day_buf[24];
    snprintf(hh, sizeof(hh), "%02d", tm_now.tm_hour);
    snprintf(mm, sizeof(mm), "%02d", tm_now.tm_min);
    snprintf(ss, sizeof(ss), "%02d", tm_now.tm_sec);
    snprintf(day_buf, sizeof(day_buf), "%02d %s", tm_now.tm_mday, get_month_ru(tm_now.tm_mon + 1));

    lv_label_set_text(ui_Label5, hh);
    lv_label_set_text(ui_Label6, mm);
    lv_label_set_text(ui_Label7, ss);
    lv_label_set_text(ui_Label1, get_weekday_ru(tm_now.tm_wday));
    lv_label_set_text(ui_Label2, day_buf);
}

static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_clock_labels();
}

// =========================================================
// ШАГОМЕР (виджет достраивается внутри существующего ui_Panel1)
// =========================================================
static void format_steps(uint32_t steps, char *buf, size_t buf_size)
{
    if (steps < 1000) {
        snprintf(buf, buf_size, "%lu", (unsigned long)steps);
        return;
    }
    char tmp[12];
    snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)steps);
    int len = strlen(tmp);
    int idx = 0;
    for (int i = 0; i < len; i++) {
        buf[idx++] = tmp[i];
        if ((len - i - 1) % 3 == 0 && i != len - 1) {
            buf[idx++] = ' ';
        }
    }
    buf[idx] = '\0';
}

static void apply_theme(void)
{
    // Базовые надписи даты/времени - основной текст темы
    lv_obj_set_style_text_color(ui_Label1, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN); // WeekDay
    lv_obj_set_style_text_color(ui_Label2, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);   // Date
    lv_obj_set_style_text_color(ui_Label5, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);   // HH
    lv_obj_set_style_text_color(ui_Label6, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);   // MM
    lv_obj_set_style_text_color(ui_Label7, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);         // SS - акцент
    lv_obj_set_style_text_color(ui_Label3, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN); // battery

    // Панель шагомера: чёрная поверхность + тонкая акцентная обводка вместо белой
    lv_obj_set_style_bg_opa(ui_Panel1, LV_OPA_TRANSP, LV_PART_MAIN);       // панель прозрачна, сливается с фоном
    lv_obj_set_style_outline_color(ui_Panel1, lv_color_hex(UI_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_outline_width(ui_Panel1, 1, LV_PART_MAIN);            // было 3 (задано в ui_ScreenHome.c)
    lv_obj_set_style_outline_opa(ui_Panel1, LV_OPA_60, LV_PART_MAIN);
}

static void build_pedometer_widget(void)
{
    // ui_Panel1 создан в ui_ScreenHome.c как пустая рамка 209x135 - достраиваем внутрь.
    ui_HomeArcSteps = lv_arc_create(ui_Panel1);
    lv_obj_set_size(ui_HomeArcSteps, 118, 118);
    lv_obj_align(ui_HomeArcSteps, LV_ALIGN_LEFT_MID, 6, 0);
    lv_arc_set_rotation(ui_HomeArcSteps, 135);
    lv_arc_set_bg_angles(ui_HomeArcSteps, 0, 270);
    lv_arc_set_range(ui_HomeArcSteps, 0, STEPS_GOAL);
    lv_arc_set_value(ui_HomeArcSteps, 0);
    lv_obj_set_style_arc_color(ui_HomeArcSteps, lv_color_hex(UI_COLOR_SURFACE_ALT), LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_HomeArcSteps, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_HomeArcSteps, lv_color_hex(UI_COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ui_HomeArcSteps, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ui_HomeArcSteps, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(ui_HomeArcSteps, true, LV_PART_INDICATOR);
    lv_obj_set_style_opa(ui_HomeArcSteps, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(ui_HomeArcSteps, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *steps_icon = lv_label_create(ui_Panel1);
    lv_label_set_text(steps_icon, LV_SYMBOL_SHUFFLE);
    lv_obj_align(steps_icon, LV_ALIGN_LEFT_MID, 44, -13);
    lv_obj_set_style_text_color(steps_icon, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);

    ui_HomeLabelSteps = lv_label_create(ui_Panel1);
    lv_label_set_text(ui_HomeLabelSteps, "0");
    lv_obj_align(ui_HomeLabelSteps, LV_ALIGN_LEFT_MID, 34, 13);
    lv_obj_set_style_text_color(ui_HomeLabelSteps, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_HomeLabelSteps, &lv_font_montserrat_20, LV_PART_MAIN);

    lv_obj_t *goal_label = lv_label_create(ui_Panel1);
    char goal_buf[24];
    snprintf(goal_buf, sizeof(goal_buf), "of %d", STEPS_GOAL);
    lv_label_set_text(goal_label, goal_buf);
    lv_obj_align(goal_label, LV_ALIGN_LEFT_MID, 34, 36);
    lv_obj_set_style_text_color(goal_label, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(goal_label, &lv_font_montserrat_14, LV_PART_MAIN);
}

// =========================================================
// ОБРАБОТЧИКИ СОБЫТИЙ
// =========================================================
static void battery_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id;
    battery_data_t *bat = (battery_data_t *)event_data;
    char buf[24];
    snprintf(buf, sizeof(buf), "%d%%\n%.2fV", bat->percent, bat->voltage);
    lv_label_set_text(ui_Label3, buf);
}

static void step_event_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id;
    if (ui_HomeLabelSteps == NULL || !lv_obj_is_valid(ui_HomeLabelSteps)) return;

    motion_step_data_t *data = (motion_step_data_t *)event_data;
    uint32_t steps = data->steps;

    char buf[16];
    format_steps(steps, buf, sizeof(buf));
    lv_label_set_text(ui_HomeLabelSteps, buf);

    uint32_t clamped = steps > STEPS_GOAL ? STEPS_GOAL : steps;
    lv_arc_set_value(ui_HomeArcSteps, (int32_t)clamped);
}

static void wrist_raise_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id; (void)event_data;
    svc_screen_sleep_wake_up();
}

static void wrist_sleep_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id; (void)event_data;
    svc_screen_sleep_force_sleep();
}

// =========================================================
// ИНИЦИАЛИЗАЦИЯ
// =========================================================
void screen_home_logic_init(void)
{
    apply_theme();
    build_pedometer_widget();
    update_clock_labels();
    lv_timer_create(clock_timer_cb, 1000, NULL);

    esp_event_handler_instance_register(APP_EVENTS, EVT_BATTERY_UPDATED, battery_event_handler, NULL, NULL);
    esp_event_handler_instance_register(APP_EVENTS, EVT_MOTION_STEP_UPDATED, step_event_handler, NULL, NULL);
    esp_event_handler_instance_register(APP_EVENTS, EVT_MOTION_WRIST_RAISE, wrist_raise_handler, NULL, NULL);
    esp_event_handler_instance_register(APP_EVENTS, EVT_MOTION_WRIST_SLEEP, wrist_sleep_handler, NULL, NULL);

    ESP_LOGI(TAG, "Home screen logic initialized");
}
