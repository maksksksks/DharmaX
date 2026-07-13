#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Сервис передачи ИК-команд по протоколу NEC через периферию RMT.
 * Несущая 38 кГц включается аппаратно самим RMT-каналом (carrier modulation),
 * так что светодиод можно вешать прямо на GPIO через транзисторный ключ.
 */

// Вызвать один раз при старте, до первого svc_ir_send_nec().
// tx_gpio - пин, на который повешен ИК-светодиод (через ключ/транзистор).
void svc_ir_transmit_init(gpio_num_t tx_gpio);

// Отправить одну NEC-посылку: адрес устройства + код команды.
// Блокирует до отправки в очередь RMT, сама передача идёт в фоне (DMA/аппаратно).
// Возвращает false, если сервис не инициализирован или очередь занята.
bool svc_ir_send_nec(uint8_t address, uint8_t command);

// Отправить "длинное нажатие" - повторяющаяся NEC repeat-посылка,
// как это делают настоящие пульты при удержании кнопки.
bool svc_ir_send_nec_repeat(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
