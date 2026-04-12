#pragma once
#include "esp_event.h"
#include <stdint.h>

ESP_EVENT_DECLARE_BASE(APP_EVENTS);

typedef enum {
    EVT_BATTERY_UPDATED,
    EVT_BLE_TIME_SYNCED,
    EVT_LIGHT_UPDATED
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