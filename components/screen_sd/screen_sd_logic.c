#include "screen_sd_logic.h"
#include "ui.h"
#include "esp_log.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "SCREEN_SD";

// Точка монтирования карты. Замените под свой драйвер SD (SPI или SDMMC 1-bit -
// см. components/docs/sd_card_and_usb.md), если он монтирует карту в другое место.
#define SD_MOUNT_POINT "/sdcard"

static char s_current_path[256] = SD_MOUNT_POINT;

static lv_obj_t *lbl_path;
static lv_obj_t *list_container;
static lv_obj_t *lbl_empty_hint;

static void refresh_listing(void);

// =========================================================
// ВСПОМОГАТЕЛЬНОЕ: человекочитаемый размер файла
// =========================================================
static void format_size(long size, char *buf, size_t buf_sz)
{
    if (size < 1024) snprintf(buf, buf_sz, "%ld B", size);
    else if (size < 1024L * 1024) snprintf(buf, buf_sz, "%.1f KB", size / 1024.0);
    else snprintf(buf, buf_sz, "%.1f MB", size / (1024.0 * 1024.0));
}

// =========================================================
// НАВИГАЦИЯ
// =========================================================
static void go_up(void)
{
    if (strcmp(s_current_path, SD_MOUNT_POINT) == 0) return; // уже в корне

    char *last_slash = strrchr(s_current_path, '/');
    if (last_slash && last_slash != s_current_path) {
        *last_slash = '\0';
    }
    refresh_listing();
}

static void go_into(const char *dirname)
{
    size_t cur_len = strlen(s_current_path);
    size_t add_len = strlen(dirname);
    if (cur_len + add_len + 2 >= sizeof(s_current_path)) return;

    if (s_current_path[cur_len - 1] != '/') strcat(s_current_path, "/");
    strcat(s_current_path, dirname);
    refresh_listing();
}

// =========================================================
// СТРОКИ СПИСКА
// =========================================================
typedef struct {
    char name[64];
    bool is_dir;
} row_user_data_t;

static void row_click_cb(lv_event_t *e)
{
    row_user_data_t *ud = (row_user_data_t *)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {   // строки пересоздаются при каждом листинге
        lv_free(ud);
        return;
    }
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (strcmp(ud->name, "..") == 0) {
        go_up();
    } else if (ud->is_dir) {
        go_into(ud->name);
    }
    // клик по файлу пока просто ничего не делает - место для предпросмотра/действий
}

static void add_row(const char *name, bool is_dir, long size)
{
    lv_obj_t *row = lv_button_create(list_container);
    lv_obj_set_width(row, lv_pct(94));
    lv_obj_set_height(row, 46);
    lv_obj_set_style_radius(row, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(UI_COLOR_SURFACE_ALT), LV_STATE_PRESSED);

    row_user_data_t *ud = lv_malloc(sizeof(row_user_data_t));
    snprintf(ud->name, sizeof(ud->name), "%s", name);
    ud->is_dir = is_dir;
    lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_ALL, ud);

    lv_obj_t *lbl_icon = lv_label_create(row);
    lv_label_set_text(lbl_icon, is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE);
    lv_obj_align(lbl_icon, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);

    lv_obj_t *lbl_name = lv_label_create(row);
    lv_label_set_text(lbl_name, name);
    lv_label_set_long_mode(lbl_name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_name, is_dir ? 190 : 140);
    lv_obj_align(lbl_name, LV_ALIGN_LEFT_MID, 40, 0);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, LV_PART_MAIN);

    if (!is_dir) {
        char size_buf[16];
        format_size(size, size_buf, sizeof(size_buf));
        lv_obj_t *lbl_size = lv_label_create(row);
        lv_label_set_text(lbl_size, size_buf);
        lv_obj_align(lbl_size, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_set_style_text_color(lbl_size, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl_size, &lv_font_montserrat_14, LV_PART_MAIN);
    }
}

// =========================================================
// ЧТЕНИЕ ДИРЕКТОРИИ
// =========================================================
static void refresh_listing(void)
{
    lv_obj_clean(list_container);
    lv_label_set_text(lbl_path, s_current_path);

    DIR *dir = opendir(s_current_path);
    if (!dir) {
        lv_obj_remove_flag(lbl_empty_hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_empty_hint, "not mounted or not founded");
        ESP_LOGW(TAG, "opendir(%s) failed", s_current_path);
        return;
    }

    if (strcmp(s_current_path, SD_MOUNT_POINT) != 0) {
        add_row("..", true, 0);
    }

    struct dirent *entry;
    int count = 0;
    char full_path[300];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", s_current_path, entry->d_name);
        struct stat st;
        bool is_dir = (entry->d_type == DT_DIR);
        long size = 0;
        if (!is_dir && stat(full_path, &st) == 0) size = (long)st.st_size;

        add_row(entry->d_name, is_dir, size);
        count++;
    }
    closedir(dir);

    if (count == 0) {
        lv_obj_remove_flag(lbl_empty_hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(lbl_empty_hint, "Пусто");
    } else {
        lv_obj_add_flag(lbl_empty_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

// =========================================================
// UI
// =========================================================
static void build_ui(void)
{
    UI_APPLY_SCREEN_BG(ui_ScreenSD);
    lv_obj_set_style_text_color(ui_Label13, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_y(ui_Label13, -210);

    lbl_path = lv_label_create(ui_ScreenSD);
    lv_label_set_text(lbl_path, SD_MOUNT_POINT);
    lv_label_set_long_mode(lbl_path, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_path, lv_pct(85));
    lv_obj_set_align(lbl_path, LV_ALIGN_TOP_MID);
    lv_obj_set_y(lbl_path, 30);
    lv_obj_set_style_text_color(lbl_path, lv_color_hex(UI_COLOR_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_path, &lv_font_montserrat_14, LV_PART_MAIN);

    list_container = lv_obj_create(ui_ScreenSD);
    lv_obj_remove_style_all(list_container);
    lv_obj_set_size(list_container, lv_pct(100), lv_pct(75));
    lv_obj_set_align(list_container, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list_container, 6, LV_PART_MAIN);
    lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_container, LV_DIR_VER);

    lbl_empty_hint = lv_label_create(ui_ScreenSD);
    lv_label_set_text(lbl_empty_hint, "");
    lv_obj_set_style_text_align(lbl_empty_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_align(lbl_empty_hint, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(lbl_empty_hint, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_obj_add_flag(lbl_empty_hint, LV_OBJ_FLAG_HIDDEN);
}

static void logic_init(void)
{
    build_ui();
    refresh_listing();
    ESP_LOGI(TAG, "SD explorer screen logic initialized");
}

static void logic_deinit(void)
{
    lbl_path = NULL;
    list_container = NULL;
    lbl_empty_hint = NULL;
    snprintf(s_current_path, sizeof(s_current_path), "%s", SD_MOUNT_POINT);   // следующий вход - с корня карты
}

lv_obj_t *screen_sd_create(void)
{
    ui_ScreenSD_screen_init();
    logic_init();
    return ui_ScreenSD;
}

void screen_sd_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenSD
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenSD_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
