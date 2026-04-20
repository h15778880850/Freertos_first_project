#include "app_main.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bsp_adc.h"
#include "bsp_log.h"
#include "bsp_oled.h"
#include "bsp_rgb.h"
#include "bsp_w25q64.h"
#include "config_service.h"
#include "monitor_service.h"

static AppRtosObjects s_rtos;
static AppConfig s_config;
static AppSelfTest s_self_test;
static bool s_config_ready;

static void app_log_enqueue(const char *fmt, ...);
static bool app_config_copy(AppConfig *config);
static bool app_config_copy_timeout(AppConfig *config, uint32_t timeout);
static void app_config_update(const AppConfig *config);
static void app_request_config_save(void);

void App_Main(const AppRtosObjects *objects)
{
  if (objects == NULL)
  {
    return;
  }

  memcpy(&s_rtos, objects, sizeof(s_rtos));
  ConfigService_Default(&s_config);
}

void sensor_task(void *argument)
{
  AppConfig config;
  uint32_t next_tick;

  (void)argument;

  BSP_Rgb_Init();
  BSP_Rgb_SetSelfTest(false);
  s_self_test.adc_ok = BSP_Adc_Init();

  while (!app_config_copy(&config))
  {
    osDelay(10U);
  }

  next_tick = osKernelGetTickCount();

  for (;;)
  {
    AppSample sample = {0};

    (void)app_config_copy(&config);
    sample.threshold = config.alarm_threshold;

    if (BSP_Adc_ReadRaw(&sample.raw))
    {
      sample.alarm_active = MonitorService_UpdateAlarm(&config, sample.raw);
      BSP_Rgb_SetAlarm(sample.alarm_active);
      (void)osMessageQueuePut(s_rtos.sample_queue, &sample, 0U, 0U);
      app_log_enqueue("sample raw=%u threshold=%u alarm=%u",
                      sample.raw,
                      sample.threshold,
                      sample.alarm_active ? 1U : 0U);
    }
    else
    {
      sample.alarm_active = true;
      BSP_Rgb_SetAlarm(true);
      (void)osMessageQueuePut(s_rtos.sample_queue, &sample, 0U, 0U);
      app_log_enqueue("sample error: adc timeout");
    }

    next_tick += config.sample_period_ms;
    (void)osDelayUntil(next_tick);
  }
}

void ui_task(void *argument)
{
  AppConfig config;
  AppSample sample;

  (void)argument;

  s_self_test.oled_ok = BSP_Oled_Init();

  while (!app_config_copy(&config))
  {
    osDelay(10U);
  }

  BSP_Oled_ShowBoot(&s_self_test);
  osDelay(1000U);

  for (;;)
  {
    if (osMessageQueueGet(s_rtos.sample_queue, &sample, NULL, osWaitForever) == osOK)
    {
      BSP_Oled_ShowSample(&sample);
    }
  }
}

void storage_task(void *argument)
{
  AppConfig loaded_config;
  AppConfig pending_config;
  uint32_t flash_id;

  (void)argument;

  s_self_test.flash_ok = BSP_W25Q64_Init();
  flash_id = BSP_W25Q64_ReadJedecId();
  s_self_test.config_restored = ConfigService_Load(&loaded_config);

  if (!s_self_test.config_restored)
  {
    ConfigService_Default(&loaded_config);
  }

  app_config_update(&loaded_config);

  app_log_enqueue("First_project monitor boot");
  app_log_enqueue("flash jedec=0x%06lX", (unsigned long)flash_id);
  app_log_enqueue("selftest: adc=%u flash=%u oled=%u cfg=%s",
                  s_self_test.adc_ok ? 1U : 0U,
                  s_self_test.flash_ok ? 1U : 0U,
                  s_self_test.oled_ok ? 1U : 0U,
                  s_self_test.config_restored ? "restored" : "default");
  app_log_enqueue("config: period=%lu threshold=%u hysteresis=%u",
                  (unsigned long)loaded_config.sample_period_ms,
                  loaded_config.alarm_threshold,
                  loaded_config.alarm_hysteresis);

  if (!s_self_test.config_restored)
  {
    app_request_config_save();
  }

  for (;;)
  {
    if (osMessageQueueGet(s_rtos.storage_queue, &pending_config, NULL, osWaitForever) == osOK)
    {
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

void log_task(void *argument)
{
  AppLogMessage message;

  (void)argument;

  s_self_test.uart_ok = BSP_Log_Init();

  for (;;)
  {
    if (osMessageQueueGet(s_rtos.log_queue, &message, NULL, osWaitForever) == osOK)
    {
      BSP_Log_Print("%s\n", message.text);
    }
  }
}

void save_timer_callback(void *argument)
{
  AppConfig config;

  (void)argument;

  if (app_config_copy_timeout(&config, 0U))
  {
    (void)osMessageQueuePut(s_rtos.storage_queue, &config, 0U, 0U);
  }
}

static void app_log_enqueue(const char *fmt, ...)
{
  AppLogMessage message;
  va_list args;
  int length;

  if ((s_rtos.log_queue == NULL) || (fmt == NULL))
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
  (void)osMessageQueuePut(s_rtos.log_queue, &message, 0U, 0U);
}

static bool app_config_copy(AppConfig *config)
{
  return app_config_copy_timeout(config, osWaitForever);
}

static bool app_config_copy_timeout(AppConfig *config, uint32_t timeout)
{
  bool ready;

  if ((config == NULL) || (s_rtos.config_mutex == NULL))
  {
    return false;
  }

  if (osMutexAcquire(s_rtos.config_mutex, timeout) != osOK)
  {
    return false;
  }

  ready = s_config_ready;
  if (ready)
  {
    *config = s_config;
  }
  (void)osMutexRelease(s_rtos.config_mutex);

  return ready;
}

static void app_config_update(const AppConfig *config)
{
  if ((config == NULL) || (s_rtos.config_mutex == NULL))
  {
    return;
  }

  (void)osMutexAcquire(s_rtos.config_mutex, osWaitForever);
  s_config = *config;
  s_config_ready = true;
  (void)osMutexRelease(s_rtos.config_mutex);
}

static void app_request_config_save(void)
{
  if (s_rtos.save_timer != NULL)
  {
    (void)osTimerStart(s_rtos.save_timer, 500U);
  }
}
