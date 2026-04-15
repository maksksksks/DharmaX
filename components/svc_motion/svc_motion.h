#pragma once
#include "driver/i2c.h"
#include "bsp_i2c.h"

void svc_motion_monitor_start(i2c_master_bus_handle_t i2c_bus_handle);
void svc_motion_monitor_stop(void);