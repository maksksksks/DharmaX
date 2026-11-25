#include <stddef.h>
#include "app_events.h"
#include "esp_event.h"

// Определяем базу событий (реальная инициализация "радиостанции")
ESP_EVENT_DEFINE_BASE(APP_EVENTS);

esp_err_t app_evt_subscribe(app_evt_sub_t *sub, int32_t id, esp_event_handler_t cb, void *arg)
{
    if (sub == NULL || cb == NULL) return ESP_ERR_INVALID_ARG;
    if (sub->inst != NULL) return ESP_ERR_INVALID_STATE;

    esp_event_handler_instance_t inst = NULL;
    esp_err_t err = esp_event_handler_instance_register(APP_EVENTS, id, cb, arg, &inst);
    if (err == ESP_OK) {
        sub->id = id;
        sub->inst = inst;
    }
    return err;
}

void app_evt_unsubscribe(app_evt_sub_t *sub)
{
    if (sub == NULL || sub->inst == NULL) return;
    esp_event_handler_instance_unregister(APP_EVENTS, sub->id, sub->inst);
    sub->inst = NULL;
}
