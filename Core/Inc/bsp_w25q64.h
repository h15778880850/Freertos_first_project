#ifndef BSP_W25Q64_H
#define BSP_W25Q64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
  BSP_W25Q64_ERROR_NONE = 0,
  BSP_W25Q64_ERROR_PARAM,
  BSP_W25Q64_ERROR_NOT_READY,
  BSP_W25Q64_ERROR_WRITE_ENABLE,
  BSP_W25Q64_ERROR_ERASE,
  BSP_W25Q64_ERROR_PROGRAM,
  BSP_W25Q64_ERROR_WAIT_READY,
} BspW25q64Error;

bool BSP_W25Q64_Init(void);
bool BSP_W25Q64_IsReady(void);
bool BSP_W25Q64_Read(uint32_t address, uint8_t *data, size_t length);
bool BSP_W25Q64_WriteSector(uint32_t address, const uint8_t *data, size_t length);
uint32_t BSP_W25Q64_ReadJedecId(void);
uint8_t BSP_W25Q64_ReadStatusReg(void);
BspW25q64Error BSP_W25Q64_GetLastError(void);

#endif
