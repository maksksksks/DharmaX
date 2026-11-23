#pragma once

#include "host/ble_hs.h"

// GAP-коллбэк (передаётся в ble_gap_adv_start)
int ble_gap_event(struct ble_gap_event *event, void *arg);