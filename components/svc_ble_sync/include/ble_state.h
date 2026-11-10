#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    // Состояние BLE
    bool running;
    bool advertising;
    bool initialized;
    volatile bool bonds_loaded;

    // Соединение и характеристики
    uint16_t active_conn_handle;
    uint16_t time_sync_conn_handle;
    uint16_t action_chr_val_handle;
    uint16_t log_chr_val_handle;
    uint8_t own_addr_type;

    // Логирование
    bool log_subscribed;
    QueueHandle_t log_queue;
    int (*original_vprintf)(const char *fmt, va_list args);

} ble_state_t;

extern ble_state_t ble_state;