#pragma once

#include <stdbool.h>

void svc_ds18b20_start();

void svc_ds18b20_get_temp(float *temp);

void svc_ds18b20_stop();

bool svc_ds18b20_is_running();