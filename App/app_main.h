#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "cmsis_os.h"
#include "app_types.h"

typedef struct
{
  char text[128];
} AppLogMessage;

typedef struct
{
  osMessageQueueId_t sample_queue;
  osMessageQueueId_t log_queue;
  osMessageQueueId_t storage_queue;
  osMutexId_t config_mutex;
  osTimerId_t save_timer;
} AppRtosObjects;

void App_Main(const AppRtosObjects *objects);
void App_SetSensorTaskHandle(osThreadId_t handle);
void App_K1PressedFromIsr(void);

void sensor_task(void *argument);
void ui_task(void *argument);
void storage_task(void *argument);
void log_task(void *argument);
void save_timer_callback(void *argument);

#endif
