#pragma once

/** Удалить ключи для текущего подключенного устройства и разорвать связь */
void svc_ble_delete_current_peer(void);

/** Удалить ВСЕ ключи сопряжения из NVS ("Забыть все устройства") */
void svc_ble_delete_all_peers(void);

/** Тест записи/чтения/удаления ключа в store. Только для отладки. */
void ble_bonds_selftest(void);