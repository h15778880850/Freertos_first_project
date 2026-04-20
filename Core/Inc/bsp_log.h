#ifndef BSP_LOG_H
#define BSP_LOG_H

#include <stdbool.h>

bool BSP_Log_Init(void);
void BSP_Log_Print(const char *fmt, ...);

#endif
