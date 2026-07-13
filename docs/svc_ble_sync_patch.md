# Патч для `svc_ble_sync.c`

В новом проекте вызовы вибро и screen-sleep были закомментированы (этих
сервисов ещё не было). Теперь `svc_vibro` и `svc_screen_sleep` добавлены -
раскомментируйте обратно.

## 1. Includes (в начале файла)

```c
#include "svc_screen_sleep.h"
#include "svc_vibro.h"
```

## 2. Внутри `time_chr_access()`

```c
svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MEDIUM, DURATION_DEFAULT);
```

## 3. Внутри `notify_chr_access()` (оба места - n_new и n_del)

```c
svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_HIGH, DURATION_LONG);   // n_new
...
svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_MEDIUM, DURATION_LONG); // n_del
```

## 4. Внутри `ble_gap_event()` (BLE_GAP_EVENT_CONNECT / DISCONNECT / ENC_CHANGE)

```c
svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);   // CONNECT
...
svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_LONG, LEVEL_HIGH, DURATION_DEFAULT);   // DISCONNECT
...
svc_vibro_send_event(VIBRATION_EVENT_SINGLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);   // ENC_CHANGE success
```

## 5. Внутри `BLE_GAP_EVENT_PASSKEY_ACTION`

```c
svc_vibro_send_event(VIBRATION_EVENT_DOUBLE_SHORT, LEVEL_MAX, DURATION_DEFAULT);
if (svc_screen_sleep_is_sleeping()) {
    svc_screen_sleep_wake_up();
}
```

## 6. `CMakeLists.txt` компонента `svc_ble_sync`

Раскомментировать (сейчас там `#svc_vibro` и `#svc_screen_sleep`):

```cmake
REQUIRES
    freertos
    log
    esp_event
    bt
    app_core
    nvs_flash
    svc_vibro
    ui
    svc_screen_sleep
    json
```

Ничего в логике JSON/GATT/NimBLE трогать не нужно - там всё уже верно
устроено и хорошо переносится 1:1.
