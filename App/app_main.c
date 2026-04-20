#include "app_main.h"

#include <stdarg.h>
#include <stdio.h>

#include "cmsis_os.h"

#include "app_types.h"
#include "bsp_adc.h"
#include "bsp_log.h"
#include "bsp_oled.h"
#include "bsp_rgb.h"
#include "bsp_w25q64.h"
#include "config_service.h"
#include "main.h"
#include "monitor_service.h"

#define APP_EVENT_CONFIG_READY 0x00000001U

typedef struct
{
  char text[128];
} AppLogMessage;

static AppConfig s_config;
static AppSelfTest s_self_test;
static osMessageQueueId_t s_sample_queue;
static osMessageQueueId_t s_log_queue;
static osMessageQueueId_t s_storage_queue;
static osEventFlagsId_t s_app_events;

static void sensor_task(void *argument);
static void ui_task(void *argument);
static void storage_task(void *argument);
static void log_task(void *argument);
static void app_log_enqueue(const char *fmt, ...);
static void app_create_tasks(void);

static const osThreadAttr_t sensor_task_attributes = {
  .name = "sensor_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

static const osThreadAttr_t ui_task_attributes = {
  .name = "ui_task",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

static const osThreadAttr_t storage_task_attributes = {
  .name = "storage_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

static const osThreadAttr_t log_task_attributes = {
  .name = "log_task",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

void App_Main(void *argument)
{
  uint32_t flash_id;

  (void)argument;

  s_sample_queue = osMessageQueueNew(4U, sizeof(AppSample), NULL);
  s_log_queue = osMessageQueueNew(12U, sizeof(AppLogMessage), NULL);
  s_storage_queue = osMessageQueueNew(2U, sizeof(AppConfig), NULL);
  s_app_events = osEventFlagsNew(NULL);

  if ((s_sample_queue == NULL) ||
      (s_log_queue == NULL) ||
      (s_storage_queue == NULL) ||
      (s_app_events == NULL))
  {
    Error_Handler();
  }

  s_self_test.uart_ok = BSP_Log_Init();
  BSP_Rgb_Init();
  BSP_Rgb_SetSelfTest(false);

  s_self_test.adc_ok = BSP_Adc_Init();
  s_self_test.flash_ok = BSP_W25Q64_Init();
  flash_id = BSP_W25Q64_ReadJedecId();

  app_create_tasks();

  app_log_enqueue("First_project monitor boot");
  app_log_enqueue("flash jedec=0x%06lX", (unsigned long)flash_id);

  osThreadExit();
}

static void app_create_tasks(void)
{
  if ((osThreadNew(log_task, NULL, &log_task_attributes) == NULL) ||
      (osThreadNew(storage_task, NULL, &storage_task_attributes) == NULL) ||
      (osThreadNew(ui_task, NULL, &ui_task_attributes) == NULL) ||
      (osThreadNew(sensor_task, NULL, &sensor_task_attributes) == NULL))
  {
    Error_Handler();
  }
}

static void app_log_enqueue(const char *fmt, ...)
{
  AppLogMessage message;
  va_list args;
  int length;

  if ((s_log_queue == NULL) || (fmt == NULL))
  {
    return;
  }

  va_start(args, fmt);
  length = vsnprintf(message.text, sizeof(message.text), fmt, args);
  va_end(args);

  if (length < 0)
  {
    return;
  }

  message.text[sizeof(message.text) - 1U] = '\0';
  (void)osMessageQueuePut(s_log_queue, &message, 0U, 0U);
}

static void sensor_task(void *argument)
{
  uint32_t next_tick;

  (void)argument;

  (void)osEventFlagsWait(s_app_events,
                         APP_EVENT_CONFIG_READY,
                         osFlagsWaitAll,
                         osWaitForever);

  next_tick = osKernelGetTickCount();

  for (;;)
  {
    AppSample sample = {0};

    sample.threshold = s_config.alarm_threshold;

    if (BSP_Adc_ReadRaw(&sample.raw))
    {
      sample.alarm_active = MonitorService_UpdateAlarm(&s_config, sample.raw);
      (void)osMessageQueuePut(s_sample_queue, &sample, 0U, 0U);
      app_log_enqueue("sample raw=%u threshold=%u alarm=%u",
                      sample.raw,
                      sample.threshold,
                      sample.alarm_active ? 1U : 0U);
    }
    else
    {
      sample.alarm_active = true;
      (void)osMessageQueuePut(s_sample_queue, &sample, 0U, 0U);
      app_log_enqueue("sample error: adc timeout");
    }

    next_tick += s_config.sample_period_ms;
    (void)osDelayUntil(next_tick);
  }
}

static void ui_task(void *argument)
{
  AppSample sample;

  (void)argument;

  s_self_test.config_restored = ConfigService_Load(&s_config);
  if (!s_self_test.config_restored)
  {
    ConfigService_Default(&s_config);
    (void)osMessageQueuePut(s_storage_queue, &s_config, 0U, 0U);
  }

  s_self_test.oled_ok = BSP_Oled_Init();
  BSP_Rgb_SetSelfTest(s_self_test.adc_ok && s_self_test.flash_ok);

  app_log_enqueue("selftest: adc=%u flash=%u oled=%u cfg=%s",
                  s_self_test.adc_ok ? 1U : 0U,
                  s_self_test.flash_ok ? 1U : 0U,
                  s_self_test.oled_ok ? 1U : 0U,
                  s_self_test.config_restored ? "restored" : "default");
  app_log_enqueue("config: period=%lu threshold=%u hysteresis=%u",
                  (unsigned long)s_config.sample_period_ms,
                  s_config.alarm_threshold,
                  s_config.alarm_hysteresis);

  BSP_Oled_ShowBoot(&s_self_test);
  osDelay(1000U);

  (void)osEventFlagsSet(s_app_events, APP_EVENT_CONFIG_READY);

  for (;;)
  {
    if (osMessageQueueGet(s_sample_queue, &sample, NULL, osWaitForever) == osOK)
    {
      BSP_Rgb_SetAlarm(sample.alarm_active);
      BSP_Oled_ShowSample(&sample);
    }
  }
}

static void storage_task(void *argument)
{
  AppConfig pending_config;

  (void)argument;

  for (;;)
  {
    if (osMessageQueueGet(s_storage_queue, &pending_config, NULL, osWaitForever) == osOK)
    {
      osDelay(500U);

      while (osMessageQueueGet(s_storage_queue, &pending_config, NULL, 0U) == osOK)
      {
      }

      if (ConfigService_Save(&pending_config))
      {
        app_log_enqueue("config saved");
      }
      else
      {
        app_log_enqueue("config save failed");
      }
    }
  }
}

static void log_task(void *argument)
{
  AppLogMessage message;

  (void)argument;

  for (;;)
  {
    if (osMessageQueueGet(s_log_queue, &message, NULL, osWaitForever) == osOK)
    {
      BSP_Log_Print("%s\n", message.text);
    }
  }
}
