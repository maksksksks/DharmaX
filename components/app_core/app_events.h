#pragma once
#include "esp_event.h"
#include <stdint.h>

ESP_EVENT_DECLARE_BASE(APP_EVENTS);

#define ACTION_MEDIA_PLAY_PAUSE 0x10
#define ACTION_MEDIA_NEXT       0x11
#define ACTION_MEDIA_PREV       0x12

typedef enum {
    EVT_NEW_NOTIFY,
    EVT_BATTERY_UPDATED,
    EVT_BLE_TIME_SYNCED,
    EVT_LIGHT_UPDATED,
    EVT_MOTION_STEP_UPDATED,
    EVT_MOTION_WRIST_RAISE,
    EVT_MOTION_WRIST_SLEEP,
    EVT_BLE_NOTIFY_RX,
    EVT_BLE_NOTIFY_DEL,
    EVT_VIBRO_STATE_CHANGED,
    EVT_TEMP_UPDATED,
    EVT_BLE_SHOW_PIN,
    EVT_BLE_CLOSE_PIN,

    EVT_BLE_EVENT_RX,     // новое событие календаря -> notify_data (переиспользуем структуру)
    EVT_BLE_EVENT_DEL,    // удаление события -> notify_del_data
    EVT_BLE_NEWS_RX,      // новость -> notify_data
    EVT_BLE_MEDIA_UPDATED,      // обновление состояния плеера -> media_data_t
    EVT_BLE_PHONE_STATUS_UPDATED, // статус телефона -> phone_status_data_t
} app_event_id_t;

// Данные, которые летят вместе с событием
typedef struct {
    int percent;
    float voltage;
} battery_data_t;

typedef struct {
    char pin[7];
} ble_pin_event_data_t;

typedef struct {
    int value;
} lighting_data_t;

typedef struct {
    float temp;
    float delta;
} temperature_data_t;

typedef enum {
    VIBRATION_EVENT_NONE = 0,

    VIBRATION_EVENT_ON,
    VIBRATION_EVENT_OFF,

    VIBRATION_EVENT_SINGLE_SHORT,
    VIBRATION_EVENT_DOUBLE_SHORT,
    VIBRATION_EVENT_TRIPLE_SHORT,

    VIBRATION_EVENT_SINGLE_LONG,
    VIBRATION_EVENT_DOUBLE_LONG,
    VIBRATION_EVENT_TRIPLE_LONG,

    VIBRATION_EVENT_SHORT_AND_LONG,
    VIBRATION_EVENT_LONG_AND_SHORT,

    VIBRATION_EVENT_ENABLE,
    VIBRATION_EVENT_DISABLE
} vibro_event_t;

typedef enum { LEVEL_ZERO = 0, LEVEL_LOW = 40, LEVEL_MEDIUM = 65, LEVEL_HIGH = 90, LEVEL_MAX = 100 } vibro_level_t;

typedef enum { DURATION_SHORT = 400, DURATION_LONG = 600, DURATION_DEFAULT = DURATION_SHORT } vibro_duration_t;

typedef struct {
    vibro_event_t event;
    vibro_level_t level;
    vibro_duration_t duration;
} vibro_data_t;

typedef struct {
    char id[64]; // Уникальный ID (sbn.key из Android)
    char grp[32];
    char app[50];   // Имя приложения (Telegram)
    char title[64]; // Заголовок или имя отправителя
    char txt[160];  // Текст уведомления
    uint32_t ts;    // Время получения (Unix timestamp)
} notify_data;

typedef struct {
    char id[64]; // ID удаляемого уведомления
    char grp[32];
} notify_del_data;

typedef struct {
    uint32_t steps;
    float intensity;
} motion_step_data_t;

typedef struct {
    uint32_t total_steps;
    float rms;
    float threshold;
    float gyro_bias_y;
    int gesture_state;  // 0=IDLE, 1=CW, 2=CCW, 3=CONFIRM
} motion_debug_info_t;

typedef struct {
    char title[64];
    char artist[64];
    uint32_t position_sec;
    uint32_t duration_sec;
    bool is_playing;
} media_data_t;

typedef struct {
    int8_t signal_percent;   // -1 если неизвестно
    int8_t battery_percent;  // -1 если неизвестно
    bool charging;
    char carrier[16];        // имя оператора, может быть ""
} phone_status_data_t;
