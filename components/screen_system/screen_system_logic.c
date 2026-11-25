#include "screen_system_logic.h"
#include "ui.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <stdio.h>

static const char *TAG = "SCREEN_SYSTEM";

static lv_obj_t *lbl_chip, *lbl_flash, *lbl_heap, *lbl_idf, *lbl_uptime;
static lv_timer_t *refresh_timer = NULL;

static void format_uptime(int64_t us, char *buf, size_t buf_sz)
{
    int64_t sec = us / 1000000;
    int h = (int)(sec / 3600);
    int m = (int)((sec / 60) % 60);
    int s = (int)(sec % 60);
    snprintf(buf, buf_sz, "%dh %02dm %02ds", h, m, s);
}

static void refresh_dynamic(void)
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%.1f KB", esp_get_free_heap_size() / 1024.0);
    lv_label_set_text(lbl_heap, buf);

    format_uptime(esp_timer_get_time(), buf, sizeof(buf));
    lv_label_set_text(lbl_uptime, buf);
}

static void refresh_timer_cb(lv_timer_t *t)
{
    (void)t;
    refresh_dynamic();
}

static lv_obj_t *make_info_row(lv_obj_t *parent, const char *label)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl_key = lv_label_create(row);
    lv_label_set_text(lbl_key, label);
    lv_obj_set_style_text_color(lbl_key, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_key, &lv_font_montserrat_14, LV_PART_MAIN);   // подписи оставляем мелкими

    lv_obj_t *lbl_val = lv_label_create(row);
    lv_label_set_text(lbl_val, "--");
    lv_obj_set_style_text_color(lbl_val, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_20, LV_PART_MAIN);   // было _14 — значения крупнее

    return lbl_val;
}

static void build_ui(void)
{
    UI_APPLY_SCREEN_BG(ui_ScreenSystem);
    lv_obj_set_style_text_color(ui_Label15, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_y(ui_Label15, -190);

    lv_obj_t *card = lv_obj_create(ui_ScreenSystem);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, lv_pct(85), 250);                                                    // было 230
    lv_obj_set_align(card, LV_ALIGN_CENTER);
    lv_obj_set_y(card, 10);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_color(card, lv_color_hex(UI_COLOR_SURFACE_DIM), LV_PART_MAIN);           // было UI_COLOR_SURFACE
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(UI_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 14, LV_PART_MAIN);                                            // было 12

    lbl_chip = make_info_row(card, LV_SYMBOL_SD_CARD " Chip");
    lbl_flash = make_info_row(card, LV_SYMBOL_SAVE " Flash");
    lbl_heap = make_info_row(card, LV_SYMBOL_BATTERY_FULL " Free heap");
    lbl_idf = make_info_row(card, LV_SYMBOL_LIST " IDF ver");
    lbl_uptime = make_info_row(card, LV_SYMBOL_REFRESH " Uptime");
}

static void refresh_static(void)
{
    esp_chip_info_t info;
    esp_chip_info(&info);

    char buf[48];
    const char *model = (info.model == CHIP_ESP32S3) ? "ESP32-S3" : "ESP32";
    snprintf(buf, sizeof(buf), "%s, %d cores, rev v%d.%d",
             model, info.cores, info.revision / 100, info.revision % 100);
    lv_label_set_text(lbl_chip, buf);

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    snprintf(buf, sizeof(buf), "%lu MB", (unsigned long)(flash_size / (1024 * 1024)));
    lv_label_set_text(lbl_flash, buf);

    lv_label_set_text(lbl_idf, esp_get_idf_version());
}

static void logic_init(void)
{
    build_ui();
    refresh_static();
    refresh_dynamic();
    refresh_timer = lv_timer_create(refresh_timer_cb, 5000, NULL);
    ESP_LOGI(TAG, "System screen logic initialized");
}

static void logic_deinit(void)
{
    if (refresh_timer) { lv_timer_delete(refresh_timer); refresh_timer = NULL; }
    lbl_chip = lbl_flash = lbl_heap = lbl_idf = lbl_uptime = NULL;
}

lv_obj_t *screen_system_create(void)
{
    ui_ScreenSystem_screen_init();
    logic_init();
    return ui_ScreenSystem;
}

void screen_system_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenSystem
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenSystem_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
