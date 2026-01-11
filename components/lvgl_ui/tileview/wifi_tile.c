#include "wifi_tile.h"
#include "bsp_wifi.h"
#include "esp_lvgl_port.h"

#define LIST_BTN_LEN_MAX 20

static lv_obj_t *list;
static lv_obj_t *label_wifi_ip;
static lv_obj_t *list_btns[LIST_BTN_LEN_MAX];
static uint16_t list_item_count = 0;
static SemaphoreHandle_t wifi_scan_semaphore;

static void update_ip_label(void)
{
    char str[50];
    if (bsp_wifi_is_connected()) {
        char ip_str[32] = {0};
        bsp_wifi_get_ip(ip_str);
        sprintf(str, "IP: %s", ip_str);
    } else {
        sprintf(str, "IP: Disconnected");
    }
    if (lvgl_port_lock(0)) {
        lv_label_set_text(label_wifi_ip, str);
        lvgl_port_unlock();
    }
}

static void wifi_timer_cb(lv_timer_t *timer)
{
    update_ip_label();
}

static void btn_wifi_scan_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    // Очищаем список
    if (lvgl_port_lock(0)) {
        for (int i = 0; i < list_item_count; i++) {
            if (list_btns[i]) {
                lv_obj_del(list_btns[i]);
                list_btns[i] = NULL;
            }
        }
        list_item_count = 0;

        list_btns[list_item_count++] = lv_list_add_btn(list, LV_SYMBOL_WIFI, "Scanning networks...");
        lvgl_port_unlock();
    }

    xSemaphoreGive(wifi_scan_semaphore);
}

static void lvgl_wifi_scan_task(void *arg)
{
    wifi_ap_record_t ap_info[LIST_BTN_LEN_MAX];
    uint16_t ap_count = 0;

    while (1) {
        xSemaphoreTake(wifi_scan_semaphore, portMAX_DELAY);

        // Включаем Wi-Fi только на время сканирования
        bsp_wifi_sta_start();

        vTaskDelay(pdMS_TO_TICKS(500)); // даём время на запуск

        ap_count = LIST_BTN_LEN_MAX;
        bool scan_success = bsp_wifi_scan(ap_info, &ap_count, LIST_BTN_LEN_MAX);

        // Сразу выключаем Wi-Fi — энергосбережение!
        bsp_wifi_sta_stop();

        if (lvgl_port_lock(0)) {
            // Удаляем сообщение "Scanning..."
            if (list_item_count > 0 && list_btns[0]) {
                lv_obj_del(list_btns[0]);
                list_btns[0] = NULL;
                list_item_count = 0;
            }

            if (scan_success && ap_count > 0) {
                for (int i = 0; i < ap_count && i < LIST_BTN_LEN_MAX; i++) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%s", ap_info[i].ssid);

                    list_btns[list_item_count] = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
                    lv_obj_t *label = lv_label_create(list_btns[list_item_count]);
                    lv_label_set_text_fmt(label, "%d dBm", ap_info[i].rssi);
                    list_item_count++;
                }

                // Если ничего не найдено
                if (ap_count == 0) {
                    lv_list_add_text(list, "No networks found");
                }
            } else {
                lv_list_add_text(list, "Scan failed");
            }

            lvgl_port_unlock();
        }

        // Обновляем IP (теперь должен быть "Disconnected")
        update_ip_label();
    }
}

void wifi_tile_init(lv_obj_t *parent)
{
    // Инициализируем Wi-Fi подсистему (но не включаем!)
    // bsp_wifi_init(NULL, NULL);

    list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(65));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 50);

    lv_obj_t *title = lv_label_create(parent);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "WiFi");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 3);

    label_wifi_ip = lv_label_create(parent);
    lv_label_set_text(label_wifi_ip, "IP: ----");
    lv_obj_align(label_wifi_ip, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, lv_pct(80), 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Scan");
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, btn_wifi_scan_event_handler, LV_EVENT_CLICKED, NULL);

    // Создаём семафор и задачу
    wifi_scan_semaphore = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(lvgl_wifi_scan_task, "wifi_scan_task", 1024 * 8, NULL, 5, NULL, 1);

    // Таймер для обновления IP
    lv_timer_create(wifi_timer_cb, 2000, NULL);

    // Первое обновление IP
    update_ip_label();
}