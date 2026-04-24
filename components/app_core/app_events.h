#pragma once
#include "esp_event.h"
#include <stdint.h>

ESP_EVENT_DECLARE_BASE(APP_EVENTS);

typedef enum {
    EVT_BATTERY_UPDATED,
    EVT_BLE_TIME_SYNCED,
    EVT_LIGHT_UPDATED,
    EVT_VIBRO_STATE_CHANGED,
    EVT_TEMP_UPDATED
    // EVT_STEPS_UPDATED
} app_event_id_t;

// Данные, которые летят вместе с событием
typedef struct {
    int percent;
    float voltage;
} battery_data_t;

typedef struct {
    int value;
} lighting_data_t;

typedef struct {
    float temp;
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

typedef enum {
    LEVEL_ZERO = 0,
    LEVEL_LOW = 25,
    LEVEL_MEDIUM = 50,
    LEVEL_HIGH = 75,
    LEVEL_MAX = 100
} vibro_level_t;

typedef enum {
    DURATION_SHORT = 100,
    DURATION_LONG = 500,
    DURATION_DEFAULT = 100
} vibro_duration_t;

typedef struct {
    vibro_event_t event;
    vibro_level_t level;
    vibro_duration_t duration;
} vibro_data_t;
