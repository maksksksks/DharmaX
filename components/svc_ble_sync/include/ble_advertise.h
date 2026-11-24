#pragma once

#include "host/ble_hs.h"

void ble_advertise_start(void);
void ble_advertise_stop(void);
void ble_advertise_restart_slow(void);   // перевод рекламы в медленный режим (из таймера)

// Callback остаётся в svc_ble_sync.c
int ble_gap_event(struct ble_gap_event *event, void *arg);