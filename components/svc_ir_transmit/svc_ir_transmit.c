#include "svc_ir_transmit.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SVC_IR";

#define IR_RESOLUTION_HZ    1000000  // 1 MHz -> 1 тик = 1 мкс, удобно считать длительности NEC
#define IR_CARRIER_FREQ_HZ  38000    // несущая NEC
#define IR_CARRIER_DUTY     0.33f

static rmt_channel_handle_t s_tx_chan = NULL;
static rmt_encoder_handle_t s_copy_encoder = NULL;
static bool s_initialized = false;

// NEC-посылка: header + 32 бита (addr, ~addr, cmd, ~cmd) + стоп-бит = 34 символа
#define NEC_SYMBOL_COUNT (1 + 32 + 1)

static void nec_build_symbols(rmt_symbol_word_t *out, uint8_t address, uint8_t command)
{
    int idx = 0;

    // Header: 9ms mark, 4.5ms space
    out[idx++] = (rmt_symbol_word_t){
        .duration0 = 9000, .level0 = 1,
        .duration1 = 4500, .level1 = 0,
    };

    uint32_t data = (uint32_t)address | ((uint32_t)(uint8_t)(~address) << 8) |
                     ((uint32_t)command << 16) | ((uint32_t)(uint8_t)(~command) << 24);

    for (int bit = 0; bit < 32; bit++) {
        bool one = (data >> bit) & 0x1;
        out[idx++] = (rmt_symbol_word_t){
            .duration0 = 560, .level0 = 1,
            .duration1 = (uint16_t)(one ? 1690 : 560), .level1 = 0,
        };
    }

    // Финальный стоп-бит (короткий mark, чтобы приёмник закрыл последний импульс)
    out[idx++] = (rmt_symbol_word_t){
        .duration0 = 560, .level0 = 1,
        .duration1 = 1, .level1 = 0,
    };
}

void svc_ir_transmit_init(gpio_num_t tx_gpio)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "already initialized");
        return;
    }

    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = tx_gpio,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = IR_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &s_tx_chan));

    rmt_carrier_config_t carrier_cfg = {
        .frequency_hz = IR_CARRIER_FREQ_HZ,
        .duty_cycle = IR_CARRIER_DUTY,
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(s_tx_chan, &carrier_cfg));

    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &s_copy_encoder));

    ESP_ERROR_CHECK(rmt_enable(s_tx_chan));

    s_initialized = true;
    ESP_LOGI(TAG, "IR transmitter ready on GPIO%d", tx_gpio);
}

static bool ir_transmit_symbols(const rmt_symbol_word_t *symbols, size_t count)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "not initialized, call svc_ir_transmit_init() first");
        return false;
    }

    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    esp_err_t err = rmt_transmit(s_tx_chan, s_copy_encoder, symbols,
                                 count * sizeof(rmt_symbol_word_t), &tx_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "rmt_transmit failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool svc_ir_send_nec(uint8_t address, uint8_t command)
{
    rmt_symbol_word_t symbols[NEC_SYMBOL_COUNT];
    nec_build_symbols(symbols, address, command);

    bool ok = ir_transmit_symbols(symbols, NEC_SYMBOL_COUNT);
    if (ok) {
        ESP_LOGI(TAG, "sent NEC addr=0x%02X cmd=0x%02X", address, command);
    }
    return ok;
}

bool svc_ir_send_nec_repeat(void)
{
    // NEC repeat: 9ms mark, 2.25ms space, 560us stop bit
    rmt_symbol_word_t symbols[2] = {
        { .duration0 = 9000, .level0 = 1, .duration1 = 2250, .level1 = 0 },
        { .duration0 = 560,  .level0 = 1, .duration1 = 1,    .level1 = 0 },
    };
    return ir_transmit_symbols(symbols, 2);
}
