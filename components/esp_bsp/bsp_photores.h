#ifndef __BSP_PHOTORES_H__
#define __BSP_PHOTORES_H__
#include <stdio.h>

#define ADC_CHANNEL_PHOTO ADC_CHANNEL_3
#define ADC_ATTEN_PHOTO ADC_ATTEN_DB_11
#define ADC_UNIT_PHOTO ADC_UNIT_1

#ifdef __cplusplus
extern "C" {
#endif

void bsp_photores_init(void);
void bsp_photores_get_voltage(int *voltage);

#ifdef __cplusplus
}
#endif


#endif