#ifndef CONFIG_SERVICE_H
#define CONFIG_SERVICE_H

#include <stdbool.h>
#include "app_types.h"

void ConfigService_Default(AppConfig *config);
bool ConfigService_Load(AppConfig *config);
bool ConfigService_Save(const AppConfig *config);

#endif
