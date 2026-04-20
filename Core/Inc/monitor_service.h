#ifndef MONITOR_SERVICE_H
#define MONITOR_SERVICE_H

#include <stdbool.h>
#include "app_types.h"

bool MonitorService_UpdateAlarm(const AppConfig *config, uint16_t raw_value);

#endif
