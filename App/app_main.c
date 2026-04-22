#include "app_main.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bsp_ds18b20.h"
#include "bsp_log.h"
#include "bsp_oled.h"
#include "bsp_rgb.h"
#include "bsp_w25q64.h"
#include "config_service.h"
#include "main.h"
#include "monitor_service.h"

#define APP_SENSOR_FLAG_K1 0x00000001U
#define APP_K1_DEBOUNCE_MS 50U
#define APP_DS18B20_READ_RETRY 3U
#define APP_K1_POLL_MS 20U

static AppRtosObjects s_rtos;
static AppConfig s_config;
static AppSelfTest s_self_test;
static bool s_config_ready;
static bool s_sensor_ready;
static osThreadId_t s_sensor_task_handle;

static void app_log_enqueue(const char *fmt, ...);
static bool app_config_copy(AppConfig *config);
static bool app_config_copy_timeout(AppConfig *config, uint32_t timeout);
static void app_config_update(const AppConfig *config);
static void app_request_config_save(void);
static bool app_read_sample(const AppConfig *config, bool k1_triggered, AppSample *sample);
static bool app_wait_period_or_k1(uint32_t period_ms);

/**
 * @brief 应用程序主入口函数
 *
 * 初始化RTOS对象并配置默认参数
 *
 * @param objects RTOS对象指针，包含队列、信号量等资源
 * @return 无
 */
void App_Main(const AppRtosObjects *objects)
{
  if (objects == NULL)
  {
    return;
  }

  memcpy(&s_rtos, objects, sizeof(s_rtos));
  ConfigService_Default(&s_config);
}

void App_SetSensorTaskHandle(osThreadId_t handle)
{
  s_sensor_task_handle = handle;
}

void App_K1PressedFromIsr(void)
{
  static uint32_t last_tick;
  uint32_t now = HAL_GetTick();

  if ((s_sensor_task_handle != NULL) && ((now - last_tick) >= APP_K1_DEBOUNCE_MS))
  {
    last_tick = now;
    (void)osThreadFlagsSet(s_sensor_task_handle, APP_SENSOR_FLAG_K1);
  }
}

/**
 * 传感器任务
 *
 * 初始化RGB和DS18B20传感器，持续读取传感器数据，
 * 处理K1按钮触发（强制采样并请求保存配置），将采样数据发送到消息队列，
 * 并根据阈值控制报警LED。
 */
void sensor_task(void *argument)
{
  AppConfig config;
  bool k1_triggered = false;

  (void)argument;

  BSP_Rgb_Init();
  BSP_Rgb_SetSelfTest(false);
  s_self_test.ds18b20_ok = BSP_DS18B20_Init();
  s_sensor_ready = true;

  while (!app_config_copy(&config))
  {
    osDelay(10U);
  }

  for (;;)
  {
    AppSample sample = {0};

    (void)app_config_copy(&config);
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET)
    {
      k1_triggered = true;
    }

    if (k1_triggered)
    {
      app_log_enqueue("K1 pressed: force sample and request config save");
      app_request_config_save();
    }

    if (app_read_sample(&config, k1_triggered, &sample))
    {
      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET)
      {
        sample.k1_triggered = true;
      }
      (void)osMessageQueuePut(s_rtos.sample_queue, &sample, 0U, 0U);
      app_log_enqueue("sample ds18b20 value=%d threshold=%u alarm=%u k1=%u",
                      sample.temperature_centi,
                      sample.threshold,
                      sample.alarm_active ? 1U : 0U,
                      sample.k1_triggered ? 1U : 0U);
      BSP_Rgb_SetAlarm(sample.alarm_active);
    }
    else
    {
      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET)
      {
        sample.k1_triggered = true;
      }
      BSP_Rgb_SetAlarm(true);
      (void)osMessageQueuePut(s_rtos.sample_queue, &sample, 0U, 0U);
      app_log_enqueue("sample error: ds18b20 failed");
    }

    k1_triggered = false;

    k1_triggered = app_wait_period_or_k1(config.sample_period_ms);
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

  while (!s_sensor_ready)
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
  s_self_test.flash_jedec_id = flash_id;
  s_self_test.config_restored = ConfigService_Load(&loaded_config);

  if (!s_self_test.config_restored)
  {
    ConfigService_Default(&loaded_config);
  }

  app_config_update(&loaded_config);

  app_log_enqueue("First_project monitor boot");
  app_log_enqueue("flash jedec=0x%06lX", (unsigned long)flash_id);
  app_log_enqueue("selftest: flash=%u oled=%u cfg=%s",
                  s_self_test.flash_ok ? 1U : 0U,
                  s_self_test.oled_ok ? 1U : 0U,
                  s_self_test.config_restored ? "restored" : "default");
  app_log_enqueue("selftest: ds18b20=%u", s_self_test.ds18b20_ok ? 1U : 0U);
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
        uint32_t jedec_id = BSP_W25Q64_ReadJedecId();
        BspW25q64Error error = BSP_W25Q64_GetLastError();
        uint8_t status = BSP_W25Q64_ReadStatusReg();
        bool ready = BSP_W25Q64_IsReady();

        app_log_enqueue("config save failed id=0x%06lX sr=0x%02X err=%u ready=%u",
                        (unsigned long)jedec_id,
                        status,
                        (unsigned int)error,
                        ready ? 1U : 0U);
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

static bool app_read_sample(const AppConfig *config, bool k1_triggered, AppSample *sample)
{
  int16_t temperature_centi;
  uint16_t alarm_value;

  if ((config == NULL) || (sample == NULL))
  {
    return false;
  }

  sample->threshold = config->alarm_threshold;
  sample->k1_triggered = k1_triggered;

  if (!s_self_test.ds18b20_ok)
  {
    s_self_test.ds18b20_ok = BSP_DS18B20_Init();
  }

  for (uint8_t retry = 0; retry < APP_DS18B20_READ_RETRY; retry++)
  {
    if (!s_self_test.ds18b20_ok)
    {
      s_self_test.ds18b20_ok = BSP_DS18B20_Init();
    }

    if (s_self_test.ds18b20_ok && BSP_DS18B20_StartConversion())
    {
      osDelay(800U);
      if (BSP_DS18B20_ReadTemperatureCenti(&temperature_centi))
      {
        s_self_test.ds18b20_ok = true;
        sample->temperature_centi = temperature_centi;
        sample->valid = true;
        alarm_value = (uint16_t)(temperature_centi >= 0 ? temperature_centi : 0);
        sample->alarm_active = MonitorService_UpdateAlarm(config, alarm_value);
        return true;
      }
    }

    osDelay(20U);
  }

  sample->valid = false;
  sample->alarm_active = true;
  return false;
}

static bool app_wait_period_or_k1(uint32_t period_ms)
{
  uint32_t elapsed = 0U;
  uint32_t last_press_tick = 0U;

  while (elapsed < period_ms)
  {
    uint32_t wait_ms = period_ms - elapsed;
    uint32_t flags;

    if (wait_ms > APP_K1_POLL_MS)
    {
      wait_ms = APP_K1_POLL_MS;
    }

    flags = osThreadFlagsWait(APP_SENSOR_FLAG_K1, osFlagsWaitAny, wait_ms);
    if ((flags & APP_SENSOR_FLAG_K1) != 0U)
    {
      return true;
    }

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET)
    {
      uint32_t now = HAL_GetTick();
      if ((now - last_press_tick) >= APP_K1_DEBOUNCE_MS)
      {
        last_press_tick = now;
        return true;
      }
    }

    elapsed += wait_ms;
  }

  return false;
}
