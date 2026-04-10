#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "home_tile.h"
#include <time.h>
#include "app_events.h"

// ПОДКЛЮЧАЕМ ФОНОВУЮ КАРТИНКУ
// Предполагается, что файл bg.c лежит в папке этого компонента и в нем объявлена переменная bg_home
extern const lv_img_dsc_t bg_home; 

static const char *TAG = "HOME_TILE";

// Глобальные указатели на объекты LVGL
lv_timer_t *home_tile_timer;
static lv_obj_t *obj_home_tile;
lv_obj_t *ui_PanelPedometer;
lv_obj_t *ui_LabelDate;
lv_obj_t *ui_LabelTimeHH;
lv_obj_t *ui_LabelTimeMM;
lv_obj_t *ui_LabelTimeSS;
lv_obj_t *ui_LabelBattery;

int dateTime[7];

// =========================================================
// ОБРАБОТЧИК СОБЫТИЙ БАТАРЕИ (Заменил getBatteryString)
// =========================================================
static void ui_battery_event_handler(void* arg, esp_event_base_t base, 
                                    int32_t id, void* event_data) {
    if (id == EVT_BATTERY_UPDATED) {
        battery_data_t *bat = (battery_data_t*) event_data;
        
        char buf[20];
        snprintf(buf, sizeof(buf), "%d%%\n%.2fV", bat->percent, bat->voltage);
        
        lv_label_set_text(ui_LabelBattery, buf);
    }
}

// static void time_sync_btn_cb(lv_event_t *e)
// {
//     ESP_LOGI(TAG, "Кнопка BLE нажата (Реклама уже идет в фоне)");
//     // В реальном приложении тут можно мигнуть иконкой Bluetooth
//     lv_obj_set_style_bg_color(btn_time_sync, lv_color_hex(0x00FF00), 0); // Зеленый = BLE работает
// }

// =========================================================
// ЛОГИКА ВРЕМЕНИ
// =========================================================
void getDateTime(int *dateTime)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    dateTime[0] = timeinfo.tm_year + 1900;
    dateTime[1] = timeinfo.tm_mon + 1;
    dateTime[2] = timeinfo.tm_mday;
    dateTime[3] = timeinfo.tm_wday;
    dateTime[4] = timeinfo.tm_hour;
    dateTime[5] = timeinfo.tm_min;
    dateTime[6] = timeinfo.tm_sec;
}

const char *getMonth(int num)
{
    switch (num) {
        case 1: return "January";
        case 2: return "February";
        case 3: return "March";
        case 4: return "April";
        case 5: return "May";
        case 6: return "June";
        case 7: return "July";
        case 8: return "August";
        case 9: return "September";
        case 10: return "October";
        case 11: return "November";
        case 12: return "December";
        default: return "---";
    }
}

// =========================================================
// ТАЙМЕР ОТРИСОВКИ (Только время, без железа!)
// =========================================================
void lv_timer_change_time_cb(lv_timer_t *timer)
{
    getDateTime(dateTime);

    char hh_buf[3], mm_buf[3], ss_buf[3], day_buf[20];
    snprintf(hh_buf, sizeof(hh_buf), "%02d", dateTime[4]);
    snprintf(mm_buf, sizeof(mm_buf), "%02d", dateTime[5]);
    snprintf(ss_buf, sizeof(ss_buf), "%02d", dateTime[6]);
    snprintf(day_buf, sizeof(day_buf), "%02d %s", dateTime[2], getMonth(dateTime[1]));

    lv_label_set_text(ui_LabelTimeHH, hh_buf);
    lv_label_set_text(ui_LabelTimeMM, mm_buf);
    lv_label_set_text(ui_LabelTimeSS, ss_buf);
    lv_label_set_text(ui_LabelDate, day_buf);
    
    // БАТАРЕЮ ТУТ БОЛЬШЕ НЕ ОБНОВЛЯЕМ! Этим занимается ui_battery_event_handler
}

// =========================================================
// СОЗДАНИЕ ОБЪЕКТОВ LVGL
// =========================================================
void home_tile_create_objects(void)
{
    ui_PanelPedometer = lv_obj_create(obj_home_tile);
    lv_obj_set_width(ui_PanelPedometer, 159);
    lv_obj_set_height(ui_PanelPedometer, 154);
    lv_obj_set_x(ui_PanelPedometer, 0);
    lv_obj_set_y(ui_PanelPedometer, 77);
    lv_obj_set_align(ui_PanelPedometer, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_PanelPedometer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_PanelPedometer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_PanelPedometer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelDate = lv_label_create(obj_home_tile);
    lv_obj_set_width(ui_LabelDate, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelDate, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelDate, -4);
    lv_obj_set_y(ui_LabelDate, -47);
    lv_obj_set_align(ui_LabelDate, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_LabelDate, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelDate, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelTimeHH = lv_label_create(obj_home_tile);
    lv_obj_set_width(ui_LabelTimeHH, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelTimeHH, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelTimeHH, -44);
    lv_obj_set_y(ui_LabelTimeHH, -129);
    lv_obj_set_align(ui_LabelTimeHH, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_LabelTimeHH, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelTimeHH, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelTimeMM = lv_label_create(obj_home_tile);
    lv_obj_set_width(ui_LabelTimeMM, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelTimeMM, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelTimeMM, -44);
    lv_obj_set_y(ui_LabelTimeMM, -81);
    lv_obj_set_align(ui_LabelTimeMM, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_LabelTimeMM, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelTimeMM, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelTimeSS = lv_label_create(obj_home_tile);
    lv_obj_set_width(ui_LabelTimeSS, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelTimeSS, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelTimeSS, 28);
    lv_obj_set_y(ui_LabelTimeSS, -103);
    lv_obj_set_align(ui_LabelTimeSS, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(ui_LabelTimeSS, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelTimeSS, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelBattery = lv_label_create(obj_home_tile);
    lv_obj_set_width(ui_LabelBattery, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_LabelBattery, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_LabelBattery, 63);
    lv_obj_set_y(ui_LabelBattery, -141);
    lv_obj_set_align(ui_LabelBattery, LV_ALIGN_CENTER);
    // Устанавливаем заглушку, реальное значение придет через 5 секунд от фоновой задачи
    lv_label_set_text(ui_LabelBattery, "--%\n--V"); 
    lv_obj_set_style_text_color(ui_LabelBattery, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
}

// =========================================================
// ИНИЦИАЛИЗАЦИЯ ТАЙЛА
// =========================================================
void home_tile_init(lv_obj_t *parent)
{
    obj_home_tile = parent;
    
    // Применяем фоновую картинку (теперь переменная bg_home известна компилятору)
    lv_obj_set_style_bg_img_src(obj_home_tile, &bg_home, LV_PART_MAIN | LV_STATE_DEFAULT);

    home_tile_create_objects();

    // Запускаем таймер времени
    home_tile_timer = lv_timer_create(lv_timer_change_time_cb, 1000, NULL);

    // Подписываемся на события батареи
    esp_event_handler_instance_t battery_handler;
    esp_event_handler_instance_register(APP_EVENTS, EVT_BATTERY_UPDATED, 
                                       ui_battery_event_handler, NULL, &battery_handler);
}