#include "settings_tile.h"
#include "esp_log.h"
#include "svc_auto_brightness.h"
#include "svc_battery_monitor.h"
#include "svc_ble_sync.h"
#include "svc_lighting_monitor.h"
#include "svc_screen_sleep.h"

static const char *TAG = "SETTINGS_TILE";

// ==========================================
// ГЛОБАЛЬНЫЕ ОБЪЕКТЫ LVGL
// ==========================================
lv_obj_t *sw_auto_brightness;
lv_obj_t *sw_screen_sleep;
lv_obj_t *sw_battery_monitor;
lv_obj_t *sw_lighting_monitor;
lv_obj_t *sw_ble_sync;

static lv_obj_t *obj_settings_tile;

// Лейблы статуса
static lv_obj_t *lbl_status_auto_brightness;
static lv_obj_t *lbl_status_screen_sleep;
static lv_obj_t *lbl_status_battery;
static lv_obj_t *lbl_status_lighting;
static lv_obj_t *lbl_status_ble;

// ==========================================
// CALLBACKs ТУМБЛЕРОВ
// ==========================================

static void sw_auto_brightness_cb(lv_event_t *e) {
    bool enabled = lv_obj_has_state(sw_auto_brightness, LV_STATE_CHECKED);

    if (enabled) {
        svc_auto_brightness_set_enabled(true);
        svc_auto_brightness_resume(true);
        lv_label_set_text(lbl_status_auto_brightness, "#00FF00 ON#");
        ESP_LOGI(TAG, "Auto-Brightness: ON");
    } else {
        svc_auto_brightness_set_enabled(false);
        lv_label_set_text(lbl_status_auto_brightness, "#FF0000 OFF#");
        ESP_LOGI(TAG, "Auto-Brightness: OFF");
    }
}

static void sw_screen_sleep_cb(lv_event_t *e) {
    bool enabled = lv_obj_has_state(sw_screen_sleep, LV_STATE_CHECKED);

    if (enabled) {
        svc_screen_sleep_set_enabled(true);
        lv_label_set_text(lbl_status_screen_sleep, "#00FF00 ON#");
        ESP_LOGI(TAG, "Screen Sleep: ON (20s)");
    } else {
        svc_screen_sleep_set_enabled(false);
        svc_screen_sleep_wake_up();
        lv_label_set_text(lbl_status_screen_sleep, "#FF0000 OFF#");
        ESP_LOGI(TAG, "Screen Sleep: OFF");
    }
}

static void sw_battery_monitor_cb(lv_event_t *e) {
    bool enabled = lv_obj_has_state(sw_battery_monitor, LV_STATE_CHECKED);

    if (enabled) {
        svc_battery_monitor_start();
        lv_label_set_text(lbl_status_battery, "#00FF00 ON#");
        ESP_LOGI(TAG, "Battery Monitor: ON");
    } else {
        svc_battery_monitor_stop();
        lv_label_set_text(lbl_status_battery, "#FF0000 OFF#");
        ESP_LOGI(TAG, "Battery Monitor: OFF");
    }
}

static void sw_lighting_monitor_cb(lv_event_t *e) {
    bool enabled = lv_obj_has_state(sw_lighting_monitor, LV_STATE_CHECKED);

    if (enabled) {
        svc_lighting_monitor_start();
        lv_label_set_text(lbl_status_lighting, "#00FF00 ON#");
        ESP_LOGI(TAG, "Lighting Monitor: ON");
    } else {
        svc_lighting_monitor_stop();
        lv_label_set_text(lbl_status_lighting, "#FF0000 OFF#");
        ESP_LOGI(TAG, "Lighting Monitor: OFF");
    }
}

static void sw_ble_sync_cb(lv_event_t *e) {
    bool enabled = lv_obj_has_state(sw_ble_sync, LV_STATE_CHECKED);

    if (enabled) {
        svc_ble_sync_start();
        lv_label_set_text(lbl_status_ble, "#00A0FF ADV#");
        ESP_LOGI(TAG, "BLE: ON (advertising)");
    } else {
        svc_ble_sync_stop();
        lv_label_set_text(lbl_status_ble, "#FF0000 OFF#");
        ESP_LOGI(TAG, "BLE: OFF");
    }
}

// ==========================================
// ВСПОМОГАТЕЛЬНАЯ: Таймер проверки BLE (вместо lambda!)
// ==========================================

static void ble_check_timer_cb(lv_timer_t *timer) {
    if (svc_ble_sync_is_advertising()) {
        lv_label_set_text(lbl_status_ble, "#00A0FF ADV#");
    } else if (svc_ble_sync_is_running()) {
        lv_label_set_text(lbl_status_ble, "#00FF00 ON#");
    }
    lv_timer_del(timer); // Удаляем себя после одного срабатывания
}

// ==========================================
// ВСПОМОГАТЕЛЬНАЯ: Создать строку настройки
// ==========================================

static void create_setting_row(lv_obj_t *parent, const char *name, const char *desc, lv_event_cb_t cb,
                               lv_obj_t **out_switch, lv_obj_t **out_label) {
    // Ряд
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(row, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(row, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Контейнер слева (текст)
    lv_obj_t *cont_left = lv_obj_create(row);
    lv_obj_set_size(cont_left, lv_pct(70), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont_left, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(cont_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(cont_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cont_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Название
    lv_obj_t *lbl_name = lv_label_create(cont_left);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(0xFFFFFF), 0);
    // ✅ ИСПРАВЛЕНО: montserrat_14 вместо несуществующего _10/_18
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, 0);

    // Описание
    lv_obj_t *lbl_desc = lv_label_create(cont_left);
    lv_label_set_text(lbl_desc, desc);
    lv_obj_set_style_text_color(lbl_desc, lv_color_hex(0x888888), 0);
    // ✅ ИСПРАВЛЕНО: montserrat_14 (самый маленький что есть)
    lv_obj_set_style_text_font(lbl_desc, &lv_font_montserrat_14, 0);

    // Контейнер справа (тумблер + статус)
    lv_obj_t *cont_right = lv_obj_create(row);
    lv_obj_set_size(cont_right, lv_pct(30), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont_right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_align(cont_right, LV_ALIGN_CENTER, 0);
    lv_obj_clear_flag(cont_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(cont_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cont_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Тумблер
    lv_obj_t *sw = lv_switch_create(cont_right);
    lv_obj_set_width(sw, 50);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Статус
    lv_obj_t *lbl = lv_label_create(cont_right);
    lv_label_set_recolor(lbl, true);
    lv_label_set_text(lbl, "---");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0); // ✅ ИСПРАВЛЕНО
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x666666), 0);

    // ✅ ИСПРАВЛЕНО: разыменование указателя!
    if (out_switch)
        *out_switch = sw;
    if (out_label)
        *out_label = lbl; // ← Было просто "lbl" без звездочки!
}

// ==========================================
// ВСПОМОГАТЕЛЬНАЯ: Заголовок секции
// ==========================================

static void create_section_header(lv_obj_t *parent, const char *text) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0); // ✅ ИСПРАВЛЕНО
    lv_obj_set_style_pad_top(lbl, 15, 0);
    lv_obj_set_style_pad_bottom(lbl, 5, 0);
}

// ==========================================
// ВСПОМОГАТЕЛЬНАЯ: Разделитель
// ==========================================

static void create_divider(lv_obj_t *parent) {
    lv_obj_t *div = lv_obj_create(parent);
    lv_obj_set_width(div, lv_pct(100));
    lv_obj_set_height(div, 1);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(div, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(div, 0, 0);
    lv_obj_set_style_pad_all(div, 0, 0);
    // ✅ ИСПРАВЛЕНО: используем pad вместо margin (margin нет в LVGL v8!)
    lv_obj_set_style_pad_top(div, 5, 0);
    lv_obj_set_style_pad_bottom(div, 5, 0);
}

// ==========================================
// СОЗДАНИЕ ОБЪЕКТОВ
// ==========================================

/**
 * @brief Создать строку настройки с АБСОЛЮТНЫМИ КООРДИНАТАМИ
 *
 * @param parent   Родительский объект
 * @param name     Название настройки
 * @param desc     Описание (подпись)
 * @param cb       Callback тумблера
 * @param out_switch  Указатель куда сохранить тумблер
 * @param out_label   Указатель куда сохранить лейбл статуса
 * @param y_pos    Y-координата строки
 */
static void create_setting_row_abs(lv_obj_t *parent, const char *name, const char *desc, lv_event_cb_t cb,
                                   lv_obj_t **out_switch, lv_obj_t **out_label, int y_pos) {
    // === Контейнер строки ===
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), 55); // Фиксированная высота 55px
    lv_obj_set_x(row, 0);
    lv_obj_set_y(row, y_pos);
    lv_obj_set_align(row, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_style_border_opa(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);

    // === Название ===
    lv_obj_t *lbl_name = lv_label_create(row);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_width(lbl_name, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl_name, LV_SIZE_CONTENT);
    lv_obj_set_x(lbl_name, 12);
    lv_obj_set_y(lbl_name, 6);
    lv_obj_set_align(lbl_name, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, 0);

    // === Описание ===
    lv_obj_t *lbl_desc = lv_label_create(row);
    lv_label_set_text(lbl_desc, desc);
    lv_obj_set_width(lbl_desc, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl_desc, LV_SIZE_CONTENT);
    lv_obj_set_x(lbl_desc, 12);
    lv_obj_set_y(lbl_desc, 26);
    lv_obj_set_align(lbl_desc, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_text_color(lbl_desc, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_montserrat_14, 0);

    // === Тумблер ===
    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 50, 24);
    lv_obj_set_x(sw, -15);
    lv_obj_set_y(sw, 16);
    lv_obj_set_align(sw, LV_ALIGN_RIGHT_MID);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);

    // === Лейбл статуса (под тумблером) ===
    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_recolor(lbl, true);
    lv_label_set_text(lbl, "---");
    lv_obj_set_width(lbl, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl, LV_SIZE_CONTENT);
    lv_obj_set_x(lbl, -25);
    lv_obj_set_y(lbl, 36);
    lv_obj_set_align(lbl, LV_ALIGN_RIGHT_MID);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x666666), 0);

    // Возвращаем указатели
    if (out_switch)
        *out_switch = sw;
    if (out_label)
        *out_label = lbl;
}

static void settings_tile_create_objects(void) {
    // =====================================================
    // ЗАГОЛОВОК (y = 5)
    // =====================================================
    lv_obj_t *lbl_icon = lv_label_create(obj_settings_tile);
    lv_label_set_text(lbl_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_width(lbl_icon, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl_icon, LV_SIZE_CONTENT);
    lv_obj_set_x(lbl_icon, 10);
    lv_obj_set_y(lbl_icon, 5);
    lv_obj_set_align(lbl_icon, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_text_color(lbl_icon, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_icon, &lv_font_montserrat_28, 0);

    lv_obj_t *lbl_title = lv_label_create(obj_settings_tile);
    lv_label_set_text(lbl_title, "Settings");
    lv_obj_set_width(lbl_title, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl_title, LV_SIZE_CONTENT);
    lv_obj_set_x(lbl_title, 45);
    lv_obj_set_y(lbl_title, 8);
    lv_obj_set_align(lbl_title, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);

    lv_obj_t *lbl_hint = lv_label_create(obj_settings_tile);
    lv_label_set_text(lbl_hint, LV_SYMBOL_LEFT " Swipe");
    lv_obj_set_width(lbl_hint, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl_hint, LV_SIZE_CONTENT);
    lv_obj_set_x(lbl_hint, -10);
    lv_obj_set_y(lbl_hint, 10);
    lv_obj_set_align(lbl_hint, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_style_text_color(lbl_hint, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_14, 0);

    // =====================================================
    // РАЗДЕЛИТЕЛЬ (y = 48)
    // =====================================================
    lv_obj_t *div1 = lv_obj_create(obj_settings_tile);
    lv_obj_set_size(div1, lv_pct(100), 1);
    lv_obj_set_x(div1, 0);
    lv_obj_set_y(div1, 48);
    lv_obj_set_align(div1, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(div1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(div1, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(div1, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(div1, 0, 0);
    lv_obj_set_style_pad_all(div1, 0, 0);

    // =====================================================
    // СЕКЦИЯ: DISPLAY (y = 58)
    // =====================================================
    lv_obj_t *sec_display = lv_label_create(obj_settings_tile);
    lv_label_set_text(sec_display, "DISPLAY");
    lv_obj_set_width(sec_display, LV_SIZE_CONTENT);
    lv_obj_set_height(sec_display, LV_SIZE_CONTENT);
    lv_obj_set_x(sec_display, 10);
    lv_obj_set_y(sec_display, 56);
    lv_obj_set_align(sec_display, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_text_color(sec_display, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(sec_display, &lv_font_montserrat_14, 0);

    // --- Строка: Auto Brightness (y = 78) ---
    create_setting_row_abs(obj_settings_tile, "Auto Brightness", "Ambient light -> screen", sw_auto_brightness_cb,
                           &sw_auto_brightness, &lbl_status_auto_brightness, 78);

    // --- Строка: Screen Sleep (y = 138) ---
    create_setting_row_abs(obj_settings_tile, "Screen Sleep", "Off after 20s idle", sw_screen_sleep_cb,
                           &sw_screen_sleep, &lbl_status_screen_sleep, 138);

    // =====================================================
    // РАЗДЕЛИТЕЛЬ (y = 198)
    // =====================================================
    lv_obj_t *div2 = lv_obj_create(obj_settings_tile);
    lv_obj_set_size(div2, lv_pct(100), 1);
    lv_obj_set_x(div2, 0);
    lv_obj_set_y(div2, 198);
    lv_obj_set_align(div2, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(div2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(div2, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(div2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(div2, 0, 0);
    lv_obj_set_style_pad_all(div2, 0, 0);

    // =====================================================
    // СЕКЦИЯ: SENSORS (y = 208)
    // =====================================================
    lv_obj_t *sec_sensors = lv_label_create(obj_settings_tile);
    lv_label_set_text(sec_sensors, "SENSORS");
    lv_obj_set_width(sec_sensors, LV_SIZE_CONTENT);
    lv_obj_set_height(sec_sensors, LV_SIZE_CONTENT);
    lv_obj_set_x(sec_sensors, 10);
    lv_obj_set_y(sec_sensors, 206);
    lv_obj_set_align(sec_sensors, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_text_color(sec_sensors, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(sec_sensors, &lv_font_montserrat_14, 0);

    // --- Строка: Battery (y = 228) ---
    create_setting_row_abs(obj_settings_tile, "Battery", "Monitor level & voltage", sw_battery_monitor_cb,
                           &sw_battery_monitor, &lbl_status_battery, 228);

    // --- Строка: Light Sensor (y = 288) ---
    create_setting_row_abs(obj_settings_tile, "Light Sensor", "Read ambient light", sw_lighting_monitor_cb,
                           &sw_lighting_monitor, &lbl_status_lighting, 288);

    // =====================================================
    // РАЗДЕЛИТЕЛЬ (y = 348)
    // =====================================================
    lv_obj_t *div3 = lv_obj_create(obj_settings_tile);
    lv_obj_set_size(div3, lv_pct(100), 1);
    lv_obj_set_x(div3, 0);
    lv_obj_set_y(div3, 348);
    lv_obj_set_align(div3, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(div3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(div3, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(div3, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(div3, 0, 0);
    lv_obj_set_style_pad_all(div3, 0, 0);

    // =====================================================
    // СЕКЦИЯ: CONNECTIVITY (y = 358)
    // =====================================================
    lv_obj_t *sec_conn = lv_label_create(obj_settings_tile);
    lv_label_set_text(sec_conn, "CONNECTIVITY");
    lv_obj_set_width(sec_conn, LV_SIZE_CONTENT);
    lv_obj_set_height(sec_conn, LV_SIZE_CONTENT);
    lv_obj_set_x(sec_conn, 10);
    lv_obj_set_y(sec_conn, 356);
    lv_obj_set_align(sec_conn, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_text_color(sec_conn, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(sec_conn, &lv_font_montserrat_14, 0);

    // --- Строка: Bluetooth (y = 378) ---
    create_setting_row_abs(obj_settings_tile, "Bluetooth", "BLE time sync", sw_ble_sync_cb, &sw_ble_sync,
                           &lbl_status_ble, 378);

    // =====================================================
    // УСТАНОВКА НАЧАЛЬНЫХ ЗНАЧЕНИЙ
    // =====================================================

    if (svc_auto_brightness_is_enabled()) {
        lv_obj_add_state(sw_auto_brightness, LV_STATE_CHECKED);
        lv_label_set_text(lbl_status_auto_brightness, "#00FF00 ON#");
    } else {
        lv_obj_clear_state(sw_auto_brightness, LV_STATE_CHECKED);
        lv_label_set_text(lbl_status_auto_brightness, "#FF0000 OFF#");
    }

    if (svc_screen_sleep_is_enabled()) {
        lv_obj_add_state(sw_screen_sleep, LV_STATE_CHECKED);
        lv_label_set_text(lbl_status_screen_sleep, "#00FF00 ON#");
    } else {
        lv_obj_clear_state(sw_screen_sleep, LV_STATE_CHECKED);
        lv_label_set_text(lbl_status_screen_sleep, "#FF0000 OFF#");
    }

    lv_obj_add_state(sw_battery_monitor, LV_STATE_CHECKED);
    lv_label_set_text(lbl_status_battery, "#00FF00 ON#");

    lv_obj_add_state(sw_lighting_monitor, LV_STATE_CHECKED);
    lv_label_set_text(lbl_status_lighting, "#00FF00 ON#");

    if (svc_ble_sync_is_running()) {
        lv_obj_add_state(sw_ble_sync, LV_STATE_CHECKED);
        lv_timer_create(ble_check_timer_cb, 2000, NULL);
    } else {
        lv_obj_clear_state(sw_ble_sync, LV_STATE_CHECKED);
        lv_label_set_text(lbl_status_ble, "#FF0000 OFF#");
    }
}

// ==========================================
// ИНИЦИАЛИЗАЦИЯ ТАЙЛА
// ==========================================

void settings_tile_init(lv_obj_t *parent) {
    obj_settings_tile = parent;

    // Фон тайла
    lv_obj_set_style_bg_color(obj_settings_tile, lv_color_hex(0x111111), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(obj_settings_tile, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Включаем скролл
    lv_obj_set_scroll_dir(obj_settings_tile, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(obj_settings_tile, LV_SCROLL_SNAP_NONE);

    // Создаём объекты
    settings_tile_create_objects();

    ESP_LOGI(TAG, "Settings tile initialized");
}