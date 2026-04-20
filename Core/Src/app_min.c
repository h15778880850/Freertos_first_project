#include "app_min.h"

#include "cmsis_os.h"

#include "app_types.h"
#include "bsp_adc.h"
#include "bsp_log.h"
#include "bsp_oled.h"
#include "bsp_rgb.h"
#include "bsp_w25q64.h"
#include "config_service.h"
#include "monitor_service.h"

void App_Main(void *argument)
{
  AppConfig config;
  AppSelfTest self_test = {0};
  uint32_t flash_id;

  (void)argument;

  self_test.uart_ok = BSP_Log_Init();
  BSP_Log_Print("\nFirst_project monitor boot\n");

  BSP_Rgb_Init();
  BSP_Rgb_SetSelfTest(false);

  self_test.adc_ok = BSP_Adc_Init();
  self_test.flash_ok = BSP_W25Q64_Init();
  flash_id = BSP_W25Q64_ReadJedecId();
  self_test.config_restored = ConfigService_Load(&config);

  if (!self_test.config_restored)
  {
    ConfigService_Default(&config);
    (void)ConfigService_Save(&config);
  }

  self_test.oled_ok = BSP_Oled_Init();
  BSP_Rgb_SetSelfTest(self_test.adc_ok && self_test.flash_ok);

  BSP_Log_Print("selftest: adc=%u flash=%u oled=%u cfg=%s jedec=0x%06lX\n",
                self_test.adc_ok ? 1U : 0U,
                self_test.flash_ok ? 1U : 0U,
                self_test.oled_ok ? 1U : 0U,
                self_test.config_restored ? "restored" : "default",
                (unsigned long)flash_id);
  BSP_Log_Print("config: period=%lu threshold=%u hysteresis=%u\n",
                (unsigned long)config.sample_period_ms,
                config.alarm_threshold,
                config.alarm_hysteresis);

  BSP_Oled_ShowBoot(&self_test);
  osDelay(1000U);

  for (;;)
  {
    AppSample sample = {0};

    sample.threshold = config.alarm_threshold;

    if (BSP_Adc_ReadRaw(&sample.raw))
    {
      sample.alarm_active = MonitorService_UpdateAlarm(&config, sample.raw);
      BSP_Rgb_SetAlarm(sample.alarm_active);
      BSP_Oled_ShowSample(&sample);
      BSP_Log_Print("sample raw=%u threshold=%u alarm=%u\n",
                    sample.raw,
                    sample.threshold,
                    sample.alarm_active ? 1U : 0U);
    }
    else
    {
      BSP_Rgb_SetAlarm(true);
      BSP_Log_Print("sample error: adc timeout\n");
    }

    osDelay(config.sample_period_ms);
  }
}
