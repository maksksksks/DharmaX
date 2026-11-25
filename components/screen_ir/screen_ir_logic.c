#include "screen_ir_logic.h"
#include "svc_ir_transmit.h"
#include "esp_log.h"
#include "ui.h"

static const char *TAG = "SCREEN_IR";

// ИК-светодиод висит на этом GPIO (через транзисторный ключ). Поменяйте под свою плату.
// ВАЖНО: GPIO4 занят под ADC1_CHANNEL_3 (батарея, см. drv_battery.c) - не используйте его тут.
#define IR_TX_GPIO GPIO_NUM_17

typedef struct {
    uint8_t address;
    uint8_t command;
} nec_code_t;

// TODO: заполните реальными кодами после считывания родных пультов
// (adресс/команда NEC). Пока везде 0x00/0x00 - заготовка под ваши значения.
// Таблица [устройство][команда].
static nec_code_t s_code_table[IR_DEVICE_COUNT][IR_CMD_COUNT] = {0};

static bool s_ir_ready = false;

static void logic_init(void)
{
    // RMT-канал у svc_ir_transmit поднимается один раз на весь uptime
    // (повторный init сервис отвергает), поэтому экран его не трогает при пересоздании.
    if (!s_ir_ready) {
        svc_ir_transmit_init(IR_TX_GPIO);
        s_ir_ready = true;
    }
    ESP_LOGI(TAG, "IR remote screen logic initialized on GPIO%d", IR_TX_GPIO);
}

void screen_ir_handle_button(ir_device_t device, ir_cmd_id_t cmd)
{
    if (device >= IR_DEVICE_COUNT || cmd >= IR_CMD_COUNT) return;

    const nec_code_t *code = &s_code_table[device][cmd];
    if (code->address == 0 && code->command == 0) {
        ESP_LOGW(TAG, "device=%d cmd=%d: код ещё не задан (0x00/0x00) - заполните s_code_table", device, cmd);
    }
    svc_ir_send_nec(code->address, code->command);
}

static void logic_deinit(void)
{
    // у экрана нет подписок и таймеров; RMT-канал сервиса живёт вместе с приложением
}

lv_obj_t *screen_ir_create(void)
{
    ui_ScreenIR_screen_init();
    logic_init();
    return ui_ScreenIR;
}

void screen_ir_destroy(lv_obj_t *scr)
{
    (void)scr;                        // экран - синглтон, его указатель лежит в ui_ScreenIR
    logic_deinit();                   // 1) подписки  2) таймеры  3) указатели на виджеты
    ui_ScreenIR_screen_destroy();     // lv_obj_del + обнуление глобальных ui_*
}
