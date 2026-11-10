#pragma once

#include "app_events.h"
#include "cJSON.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "esp_random.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "ui.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"
#include "svc_screen_sleep.h"
#include "svc_vibro.h"
#include "sys/time.h"
#include <freertos/queue.h>

// Структура пакета лога
typedef struct {
    uint32_t timestamp_ms;
    uint8_t level;
    char tag[12];    // Короткий тег (большинство тегов в проекте ~8-10 символов)
    char msg[180];   // Сообщение с запасом
} ble_log_item_t;

int parse_esp_log_line(const char *full_msg, ble_log_item_t *item);

void ble_log_stream_init(void);
void ble_log_sender_start(void);