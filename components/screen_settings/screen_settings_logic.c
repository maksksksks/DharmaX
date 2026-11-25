#include "screen_settings_logic.h"
#include "ui.h"
#include "esp_log.h"
#include "svc_auto_brightness.h"
#include "svc_battery_monitor.h"
#include "svc_ble_sync.h"
#include "svc_lighting_monitor.h"
#include "svc_screen_sleep.h"

// Портировано 1:1 по поведению со старой платы (settings_tile.c), только
// адаптировано под LVGL9 и оформлено под новую тему.

static const char *TAG = "SCREEN_SETTINGS";

static lv_obj_t *sw_auto_brightness, *sw_screen_sleep, *sw_battery_monitor, *sw_lighting_monitor, *sw_ble_sync;
static lv_obj_t *lbl_status_auto_brightness, *lbl_status_screen_sleep, *lbl_status_battery, *lbl_status_lighting, *lbl_status_ble;
static lv_obj_t *scroll_container;
static lv_timer_t *s_ble_check_timer = NULL;

// =========================================================
// CALLBACK-и тумблеров
// =========================================================
static void set_status_label(lv_obj_t *lbl, bool on, const char *on_text)
{
    lv_label_set_text(lbl, on ? on_text : "OFF");
    lv_obj_set_style_text_color(lbl, lv_color_hex(on ? UI_COLOR_ACCENT : UI_COLOR_TEXT_DISABLED), LV_PART_MAIN);
}

static void sw_auto_brightness_cb(lv_event_t *e)
{
    (void)e;
    bool enabled = lv_obj_has_state(sw_auto_brightness, LV_STATE_CHECKED);
    if (enabled) {
        svc_auto_brightness_set_enabled(true);
        svc_auto_brightness_resume(true);
        send_action_to_phone(0x02);
    } else {
        svc_auto_brightness_set_enabled(false);
        send_action_to_phone(0x01);
    }
    set_status_label(lbl_status_auto_brightness, enabled, "ON");
    ESP_LOGI(TAG, "Auto-Brightness: %s", enabled ? "ON" : "OFF");
}

static void sw_screen_sleep_cb(lv_event_t *e)
{
    (void)e;
    bool enabled = lv_obj_has_state(sw_screen_sleep, LV_STATE_CHECKED);
    if (enabled) {
        svc_screen_sleep_set_enabled(true);
    } else {
        svc_screen_sleep_set_enabled(false);
        svc_screen_sleep_wake_up();
    }
    set_status_label(lbl_status_screen_sleep, enabled, "ON");
    ESP_LOGI(TAG, "Screen Sleep: %s", enabled ? "ON" : "OFF");
}

static void sw_battery_monitor_cb(lv_event_t *e)
{
    (void)e;
    bool enabled = lv_obj_has_state(sw_battery_monitor, LV_STATE_CHECKED);
    if (enabled) svc_battery_monitor_start(); else svc_battery_monitor_stop();
    set_status_label(lbl_status_battery, enabled, "ON");
}

static void sw_lighting_monitor_cb(lv_event_t *e)
{
    (void)e;
    bool enabled = lv_obj_has_state(sw_lighting_monitor, LV_STATE_CHECKED);
    if (enabled) svc_lighting_monitor_start(); else svc_lighting_monitor_stop();
    set_status_label(lbl_status_lighting, enabled, "ON");
}

static void sw_ble_sync_cb(lv_event_t *e)
{
    (void)e;
    bool enabled = lv_obj_has_state(sw_ble_sync, LV_STATE_CHECKED);
    if (enabled) {
        svc_ble_sync_start();
        set_status_label(lbl_status_ble, true, "ADV");
    } else {
        svc_ble_sync_stop();
        set_status_label(lbl_status_ble, false, "OFF");
    }
}

static void ble_check_timer_cb(lv_timer_t *timer)
{
    if (svc_ble_sync_is_advertising()) set_status_label(lbl_status_ble, true, "ADV");
    else if (svc_ble_sync_is_running()) set_status_label(lbl_status_ble, true, "ON");
    lv_timer_delete(timer);
    s_ble_check_timer = NULL;   // одноразовый таймер, удаляет сам себя
}

static void delete_keys_btn_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Удаляем все ключи сопряжения из памяти");
    svc_ble_delete_all_peers();
}

// =========================================================
// ПОСТРОЕНИЕ СТРОКИ-НАСТРОЙКИ
// =========================================================
static void create_setting_row(const char *name, const char *desc, lv_event_cb_t cb,
                               lv_obj_t **out_switch, lv_obj_t **out_label)
{
    lv_obj_t *row = lv_obj_create(scroll_container);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), 74);                              // было 64
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_SURFACE_DIM), LV_PART_MAIN); // было UI_COLOR_SURFACE
    lv_obj_set_style_bg_opa(row, 255, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 12, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_name = lv_label_create(row);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_pos(lbl_name, 14, 8);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_20, 0);    // было montserrat_14

    lv_obj_t *lbl_desc = lv_label_create(row);
    lv_label_set_text(lbl_desc, desc);
    lv_obj_set_pos(lbl_desc, 14, 42);                                   // было y=30
    lv_obj_set_style_text_color(lbl_desc, lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(lbl_desc, &lv_font_montserrat_14, 0);

    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 50, 26);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -50, -10);                     // было -8
    lv_obj_set_style_bg_color(sw, lv_color_hex(UI_COLOR_SURFACE_ALT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *lbl_status = lv_label_create(row);
    lv_label_set_text(lbl_status, "OFF");
    lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -12, -10);             // было -8
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);

    *out_switch = sw;
    *out_label = lbl_status;
}

static void build_ui(void)
{
    UI_APPLY_SCREEN_BG(ui_ScreenSettings);
    lv_obj_add_flag(ui_Label14, LV_OBJ_FLAG_HIDDEN); // заменяем сгенерированный заголовок своим макетом

    lv_obj_t *lbl_title = lv_label_create(ui_ScreenSettings);
    lv_label_set_text(lbl_title, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_set_align(lbl_title, LV_ALIGN_TOP_MID);
    lv_obj_set_y(lbl_title, 10);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, LV_PART_MAIN);

    scroll_container = lv_obj_create(ui_ScreenSettings);
    lv_obj_remove_style_all(scroll_container);
    lv_obj_set_size(scroll_container, lv_pct(92), lv_pct(84));
    lv_obj_set_align(scroll_container, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_flex_flow(scroll_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(scroll_container, 10, LV_PART_MAIN);
    lv_obj_add_flag(scroll_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll_container, LV_DIR_VER);

    create_setting_row("Auto Brightness", "Ambient light -> screen", sw_auto_brightness_cb,
                       &sw_auto_brightness, &lbl_status_auto_brightness);
    create_setting_row("Screen Sleep", "Off after 20s idle", sw_screen_sleep_cb,
                       &sw_screen_sleep, &lbl_status_screen_sleep);
    create_setting_row("Battery", "Monitor level & voltage", sw_battery_monitor_cb,
                       &sw_battery_monitor, &lbl_status_battery);
    create_setting_row("Light Sensor", "Read ambient light", sw_lighting_monitor_cb,
                       &sw_lighting_monitor, &lbl_status_lighting);
    create_setting_row("Bluetooth", "BLE time sync", sw_ble_sync_cb,
                       &sw_ble_sync, &lbl_status_ble);

    lv_obj_t *btn_delete = lv_button_create(scroll_container);
    lv_obj_set_size(btn_delete, lv_pct(100), 50);
    lv_obj_set_style_bg_color(btn_delete, lv_color_hex(UI_COLOR_DANGER), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_delete, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_delete, delete_keys_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_delete = lv_label_create(btn_delete);
    lv_label_set_text(lbl_delete, "Delete pairing keys");
    lv_obj_center(lbl_delete);
    lv_obj_set_style_text_color(lbl_delete, lv_color_hex(0x000000), LV_PART_MAIN);
}

// =========================================================
// НАЧАЛЬНЫЕ ЗНАЧЕНИЯ
// =========================================================
static void apply_initial_state(void)
{
    if (svc_auto_brightness_is_enabled()) {
        lv_obj_add_state(sw_auto_brightness, LV_STATE_CHECKED);
        set_status_label(lbl_status_auto_brightness, true, "ON");
    } else {
        set_status_label(lbl_status_auto_brightness, false, "OFF");
    }

    if (svc_screen_sleep_is_enabled()) {
        lv_obj_add_state(sw_screen_sleep, LV_STATE_CHECKED);
        set_status_label(lbl_status_screen_sleep, true, "ON");
    } else {
        set_status_label(lbl_status_screen_sleep, false, "OFF");
    }

    lv_obj_add_state(sw_battery_monitor, LV_STATE_CHECKED);
    set_status_label(lbl_status_battery, true, "ON");

    lv_obj_add_state(sw_lighting_monitor, LV_STATE_CHECKED);
    set_status_label(lbl_status_lighting, true, "ON");

    if (svc_ble_sync_is_running()) {
        lv_obj_add_state(sw_ble_sync, LV_STATE_CHECKED);
        s_ble_check_timer = lv_timer_create(ble_check_timer_cb, 2000, NULL);
    } else {
        set_status_label(lbl_status_ble, false, "OFF");
    }
}

static void logic_init(void)
{
    build_ui();
    apply_initial_state();
    ESP_LOGI(TAG, "Settings screen logic initialized (перенесено со старой платы)");
}

static void logic_deinit(void)
{
    if (s_ble_check_timer) { lv_timer_delete(s_ble_check_timer); s_ble_check_timer = NULL; }
    sw_auto_brightness = sw_screen_sleep = sw_battery_monitor = sw_lighting_monitor = sw_ble_sync = NULL;
    lbl_status_auto_brightness = lbl_status_screen_sleep = lbl_status_battery = NULL;
    lbl_status_lighting = lbl_status_ble = NULL;
    scroll_container = NULL;
}

lv_obj_t *screen_settings_create(void)
{
    ui_ScreenSettings_screen_init();
    logic_init();
    return ui_ScreenSettings;
}

void screen_settings_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenSettings
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenSettings_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
