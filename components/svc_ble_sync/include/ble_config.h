#pragma once

/** Настройка SM: bonding, MITM, LE Secure Connections, раздача ключей */
void ble_config_security(void);

/** TX Power +9 dBm для default/adv/conn */
void ble_config_tx_power(void);