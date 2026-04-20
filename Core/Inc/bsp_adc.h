#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdbool.h>
#include <stdint.h>

bool BSP_Adc_Init(void);
bool BSP_Adc_ReadRaw(uint16_t *value);

#endif
