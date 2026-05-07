#ifndef BSP_EC11_H
#define BSP_EC11_H

#include <stdbool.h>
#include <stdint.h>

#include "cmsis_os.h"

typedef enum
{
  EC11_NONE = 0,
  EC11_CW,
  EC11_CCW,
} Ec11Direction;

#define EC11_FLAG_ROTATION 0x00000001U

void BSP_EC11_Init(void);
void BSP_EC11_SetTaskHandle(osThreadId_t handle);
int32_t BSP_EC11_GetRotation(void);
bool BSP_EC11_IsKeyPressed(void);
void BSP_EC11_HandleS1Isr(void);

#endif
