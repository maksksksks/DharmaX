#pragma once

#include <stdbool.h>
#include <stdio.h>

/**
 * @file svc_ble_sync.h
 * @brief Сервис Bluetooth Low Energy для синхронизации времени
 */

// Запуск BLE сервиса (оригинальная функция)
void svc_ble_sync_start(void);

// ★★★ НОВЫЕ: управление сервисом ★★★
bool svc_ble_sync_is_running(void);
void svc_ble_sync_advertise_stop(void);
void svc_ble_sync_advertise_start(void);
bool svc_ble_sync_is_advertising(void);

// Пустышка для совместимости с settings_tile (stop ничего не делает на самом деле)
void svc_ble_sync_stop(void);

void send_action_to_phone(uint8_t action_code);
void svc_ble_delete_all_peers(void);
