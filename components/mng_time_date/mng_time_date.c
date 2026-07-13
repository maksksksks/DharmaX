#include "ui.h"
#include "esp_log.h"
#include "lvgl.h"
#include <time.h>
#include <stdio.h>

static const char *TAG = "TIME_DATE";

// Глобальный указатель на таймер
static lv_timer_t *home_tile_timer = NULL;

// =========================================================
// ЛОГИКА ВРЕМЕНИ И ДАТЫ
// =========================================================
static void getDateTime(int *dateTime) {
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
/*
static const char *getMonth(int num) {
    switch (num) {
    case 1: return "Января";   case 2: return "Февраля";
    case 3: return "Марта";    case 4: return "Апреля";
    case 5: return "Мая";      case 6: return "Июня";
    case 7: return "Июля";     case 8: return "Августа";
    case 9: return "Сентября"; case 10: return "Октября";
    case 11: return "Ноября";  case 12: return "Декабря";
    default: return "Ошибка!";
    }
}*/

static const char *getMonth(int num) {
    switch (num) {
    case 1: return "January";   case 2: return "Febuary";
    case 3: return "March";    case 4: return "April";
    case 5: return "May";      case 6: return "June";
    case 7: return "July";     case 8: return "August";
    case 9: return "September"; case 10: return "October";
    case 11: return "November";  case 12: return "December";
    default: return "Error!";
    }
}
/*
static const char *getDayOfWeek(int num) {
    switch (num) {
    case 0: return "Воскресенье"; case 1: return "Понедельник";
    case 2: return "Вторник";     case 3: return "Среда";
    case 4: return "Четверг";     case 5: return "Пятница";
    case 6: return "Суббота";
    default: return "Ошибка!";
    }
}*/

static const char *getDayOfWeek(int num) {
    switch (num) {
    case 0: return "Sunday"; case 1: return "Monday";
    case 2: return "Tuesday";     case 3: return "Wednesday";
    case 4: return "Thursday";     case 5: return "Friday";
    case 6: return "Saturday";
    default: return "Error!";
    }
}

// Функция обновления всех лейблов
static void update_time_and_date(void) {
    int dateTime[7];
    getDateTime(dateTime);

    char hh_buf[4], mm_buf[4], ss_buf[4];
    char week_day_buf[32], month_day_buf[32];

    snprintf(hh_buf, sizeof(hh_buf), "%02d", dateTime[4]);
    snprintf(mm_buf, sizeof(mm_buf), "%02d", dateTime[5]);
    snprintf(ss_buf, sizeof(ss_buf), "%02d", dateTime[6]);
    
    snprintf(week_day_buf, sizeof(week_day_buf), "%s", getDayOfWeek(dateTime[3]));
    snprintf(month_day_buf, sizeof(month_day_buf), "%02d %s", dateTime[2], getMonth(dateTime[1]));

    // Обновляем время (проверка на NULL защищает от краша, если экран удален)
    if (ui_Label5 != NULL) lv_label_set_text(ui_Label5, hh_buf);
    if (ui_Label6 != NULL) lv_label_set_text(ui_Label6, mm_buf);
    if (ui_Label7 != NULL) lv_label_set_text(ui_Label7, ss_buf);
    
    // Обновляем дату и день недели
    if (ui_Label1 != NULL) lv_label_set_text(ui_Label1, week_day_buf);
    if (ui_Label2 != NULL) lv_label_set_text(ui_Label2, month_day_buf);
}

// Обработчик пробуждения экрана (для svc_screen_sleep)
void on_screen_wake_update_time(void) {
    ESP_LOGI(TAG, "🌅 Wake up! Updating time NOW...");
    update_time_and_date();
}

// Коллбэк таймера LVGL (вызывается каждую секунду)
static void lv_timer_change_time_cb(lv_timer_t *timer) {
    update_time_and_date();
}

// Инициализация таймера
void home_tile_time_init(void) {
    // Создаем таймер, который будет срабатывать каждую секунду (1000 мс)
    home_tile_timer = lv_timer_create(lv_timer_change_time_cb, 1000, NULL);
    
    // Сразу обновляем время при инициализации, чтобы не ждать первую секунду
    update_time_and_date();
    ESP_LOGI(TAG, "✅ Time & Date timer initialized");
}