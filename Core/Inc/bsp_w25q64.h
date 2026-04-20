#ifndef BSP_W25Q64_H
#define BSP_W25Q64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool BSP_W25Q64_Init(void);
bool BSP_W25Q64_Read(uint32_t address, uint8_t *data, size_t length);
bool BSP_W25Q64_WriteSector(uint32_t address, const uint8_t *data, size_t length);
uint32_t BSP_W25Q64_ReadJedecId(void);

#endif
