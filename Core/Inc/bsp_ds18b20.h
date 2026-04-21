#ifndef BSP_DS18B20_H
#define BSP_DS18B20_H

#include <stdbool.h>
#include <stdint.h>

bool BSP_DS18B20_Init(void);
bool BSP_DS18B20_StartConversion(void);
bool BSP_DS18B20_ReadTemperatureCenti(int16_t *temperature_centi);

#endif
