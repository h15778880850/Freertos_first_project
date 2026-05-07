#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdbool.h>
#include "app_types.h"

bool BSP_Oled_Init(void);
void BSP_Oled_ShowSample(const AppSample *sample);
void BSP_Oled_ShowLoading(uint8_t step);
void BSP_Oled_ShowMenu(uint8_t selection, uint8_t module_count,
                       const char *const *module_names);
void BSP_Oled_ShowFlashInfo(uint32_t total_kb, uint32_t used_kb);

#endif
