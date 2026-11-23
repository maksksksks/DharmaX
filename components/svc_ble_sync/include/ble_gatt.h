#pragma once

/**
 * @brief Регистрирует GATT-сервисы (count_cfg + add_svcs).
 * @return 0 при успехе, иначе код ошибки NimBLE
 *
 * send_action_to_phone() объявлена в svc_ble_sync.h
 */
int ble_gatt_init(void);