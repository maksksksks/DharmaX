#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_sntp.h"
#include "esp_event.h"
#include "bsp_wifi.h"
#include "home_tile.h"
#include <time.h>
#include "bsp_battery.h"

static const char *TAG = "HOME_TILE";

lv_timer_t *home_tile_timer;
static lv_obj_t *obj_home_tile;
lv_obj_t *ui_PanelPedometer;
lv_obj_t *ui_LabelDate;
lv_obj_t *ui_LabelTimeHH;
lv_obj_t *ui_LabelTimeMM;
lv_obj_t *ui_LabelTimeSS;
lv_obj_t *ui_LabelBattery;
static lv_obj_t *btn_time_sync;
static lv_obj_t *label_time_sync;

static bool time_synced = false;
static bool wifi_active = false;           // Флаг: включён ли сейчас Wi-Fi
static int last_battery_percent = -1;      // Последний валидный процент
static float last_voltage = 0.0f;          // Последнее валидное напряжение

// Пин остаётся на ADC2 (GPIO12) — по вашей просьбе
#define BAT_PIN 12

// Таблицы напряжения и процентов (без изменений)
static const float voltage_table[] = {
    4.14f, 4.13f, 4.12f, 4.11f, 4.10f, 4.09f, 4.08f, 4.07f, 4.06f, 4.05f,
    4.04f, 4.03f, 4.02f, 4.01f, 4.00f, 3.99f, 3.98f, 3.97f, 3.96f, 3.95f,
    3.94f, 3.93f, 3.92f, 3.91f, 3.90f, 3.89f, 3.88f, 3.87f, 3.86f, 3.85f,
    3.84f, 3.83f, 3.82f, 3.81f, 3.80f, 3.79f, 3.78f, 3.77f, 3.76f, 3.75f,
    3.74f, 3.73f, 3.72f, 3.71f, 3.70f, 3.69f, 3.68f, 3.67f, 3.66f, 3.65f,
    3.64f, 3.63f, 3.62f, 3.61f, 3.60f, 3.59f, 3.58f, 3.57f, 3.56f, 3.55f,
    3.54f, 3.53f, 3.52f, 3.51f, 3.50f, 3.49f, 3.48f, 3.47f, 3.46f, 3.45f,
    3.44f, 3.43f, 3.42f, 3.41f, 3.40f, 3.39f, 3.38f, 3.37f, 3.36f, 3.35f,
    3.34f, 3.33f, 3.32f, 3.31f, 3.30f, 3.28f, 3.26f, 3.24f, 3.22f, 3.20f,
    3.18f, 3.16f, 3.14f, 3.12f, 3.10f, 3.05f, 3.00f, 2.95f, 2.90f, 2.80f,
    2.70f, 2.60f, 2.50f
};

static const float percent_table[] = {
    100.0f, 99.0f, 98.0f, 97.0f, 96.0f, 95.0f, 94.0f, 93.0f, 92.0f, 91.0f,
    90.0f, 89.0f, 88.0f, 87.0f, 86.0f, 85.0f, 84.0f, 83.0f, 82.0f, 81.0f,
    80.0f, 79.0f, 78.0f, 77.0f, 76.0f, 75.0f, 74.0f, 73.0f, 72.0f, 71.0f,
    70.0f, 69.0f, 68.0f, 67.0f, 66.0f, 65.0f, 64.0f, 63.0f, 62.0f, 61.0f,
    60.0f, 59.0f, 58.0f, 57.0f, 56.0f, 55.0f, 54.0f, 53.0f, 52.0f, 51.0f,
    50.0f, 49.0f, 48.0f, 47.0f, 46.0f, 45.0f, 44.0f, 43.0f, 42.0f, 41.0f,
    40.0f, 39.0f, 38.0f, 37.0f, 36.0f, 35.0f, 34.0f, 33.0f, 32.0f, 31.0f,
    30.0f, 29.0f, 28.0f, 27.0f, 26.0f, 25.0f, 24.0f, 23.0f, 22.0f, 21.0f,
    20.0f, 19.0f, 18.0f, 17.0f, 16.0f, 15.0f, 14.0f, 13.0f, 12.0f, 11.0f,
    10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f,
    0.5f, 0.2f, 0.0f
};

#define TABLE_SIZE (sizeof(voltage_table) / sizeof(voltage_table[0]))

int dateTime[7];

// Event group для Wi-Fi
static EventGroupHandle_t wifi_event_group;
#define WIFI_GOT_IP_BIT BIT0

// Callback: время успешно синхронизировано
static void time_sync_notification_cb(struct timeval *tv)
{
    time_synced = true;
    wifi_active = false;  // Wi-Fi скоро выключится
    ESP_LOGI(TAG, "Time synchronized! WiFi will be turned off.");

    esp_wifi_disconnect();
    esp_wifi_stop();
    ESP_LOGI(TAG, "WiFi turned off.");
}

// Обработчик событий Wi-Fi
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_GOT_IP_BIT);
        wifi_active = true;
    }
}

void start_time_sync(void)
{
    if (time_synced) {
        ESP_LOGI(TAG, "Time already synced");
        return;
    }

    ESP_LOGI(TAG, "Starting time sync...");

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    bsp_wifi_init("Redmi 10", "77777777");

    // Ждём получения IP
    if (xEventGroupWaitBits(wifi_event_group, WIFI_GOT_IP_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(20000)) == 0) {
        ESP_LOGE(TAG, "Failed to connect to WiFi");
        esp_wifi_stop();
        wifi_active = false;
        return;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP started, waiting for sync...");
}

static void time_sync_btn_cb(lv_event_t *e)
{
    start_time_sync();
}

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

// Безопасное чтение напряжения (только если Wi-Fi выключен)
static bool read_battery_safe(void)
{
    if (wifi_active) {
        return false;  // Не пытаемся читать — вернёт мусор из-за конфликта ADC2 + WiFi
    }

    float analogVolts = 0;
    bsp_battery_get_voltage(&analogVolts, NULL);  // Просто вызываем, без проверки возврата

    // Если функция возвращает сырое значение в мВ или другом формате — адаптируйте
    float voltage = analogVolts * 3.0f / 1000.0f;  // Если analogVolts в мВ

    int percent = 0;
    if (voltage >= voltage_table[0]) {
        percent = 100;
    } else if (voltage <= voltage_table[TABLE_SIZE - 1]) {
        percent = 0;
    } else {
        for (int i = 0; i < TABLE_SIZE - 1; i++) {
            float v_high = voltage_table[i];
            float v_low  = voltage_table[i + 1];
            if (voltage <= v_high && voltage >= v_low) {
                float p_high = percent_table[i];
                float p_low  = percent_table[i + 1];
                percent = (int)(p_low + (voltage - v_low) * (p_high - p_low) / (v_high - v_low) + 0.5f);
                break;
            }
        }
    }

    last_battery_percent = percent;
    last_voltage = voltage;
    return true;
}

const char *getBatteryString() {
    static char str_buffer[20];

    if (wifi_active) {
        // Wi-Fi включён — показываем последнее известное значение или "---"
        if (last_battery_percent >= 0) {
            snprintf(str_buffer, sizeof(str_buffer), "%d%%\n%.2fV", last_battery_percent, last_voltage);
        } else {
            snprintf(str_buffer, sizeof(str_buffer), "--%%\n--V");
        }
    } else {
        // Wi-Fi выключен — можно читать актуальное значение
        if (read_battery_safe()) {
            snprintf(str_buffer, sizeof(str_buffer), "%d%%\n%.2fV", last_battery_percent, last_voltage);
        } else if (last_battery_percent >= 0) {
            snprintf(str_buffer, sizeof(str_buffer), "%d%%\n%.2fV", last_battery_percent, last_voltage);
        } else {
            snprintf(str_buffer, sizeof(str_buffer), "--%%\n--V");
        }
    }
    return str_buffer;
}

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

    // Обновляем батарею — безопасно
    lv_label_set_text(ui_LabelBattery, getBatteryString());
}

// Остальной код создания объектов — без изменений
void home_tile_create_objects(void)
{
    // ... (тот же код, что и раньше)
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
    lv_label_set_text(ui_LabelBattery, "--%\n--V");
    lv_obj_set_style_text_color(ui_LabelBattery, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    btn_time_sync = lv_btn_create(obj_home_tile);
    lv_obj_set_size(btn_time_sync, 48, 48);
    lv_obj_align(btn_time_sync, LV_ALIGN_BOTTOM_LEFT, 6, -6);
    lv_obj_add_event_cb(btn_time_sync, time_sync_btn_cb, LV_EVENT_CLICKED, NULL);

    label_time_sync = lv_label_create(btn_time_sync);
    lv_label_set_text(label_time_sync, LV_SYMBOL_REFRESH);
    lv_obj_center(label_time_sync);
}

void home_tile_init(lv_obj_t *parent)
{
    obj_home_tile = parent;
    lv_obj_set_style_bg_img_src(obj_home_tile, &bg_home, LV_PART_MAIN | LV_STATE_DEFAULT);

    home_tile_create_objects();

    home_tile_timer = lv_timer_create(lv_timer_change_time_cb, 1000, NULL);

    // При старте Wi-Fi выключен — сразу читаем батарею один раз
    read_battery_safe();
    lv_label_set_text(ui_LabelBattery, getBatteryString());
}