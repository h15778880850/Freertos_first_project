#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdbool.h>
#include "app_types.h"

bool BSP_Oled_Init(void);
void BSP_Oled_ShowBoot(const AppSelfTest *self_test);
void BSP_Oled_ShowSample(const AppSample *sample);

#endif
