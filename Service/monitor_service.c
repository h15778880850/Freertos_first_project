#include "monitor_service.h"

bool MonitorService_UpdateAlarm(const AppConfig *config, uint16_t raw_value)
{
  static bool alarm_active;
  uint16_t clear_threshold;

  if (config == 0)
  {
    return false;
  }

  clear_threshold = (config->alarm_threshold > config->alarm_hysteresis)
                    ? (uint16_t)(config->alarm_threshold - config->alarm_hysteresis)
                    : 0U;

  if (raw_value >= config->alarm_threshold)
  {
    alarm_active = true;
  }
  else if (raw_value <= clear_threshold)
  {
    alarm_active = false;
  }

  return alarm_active;
}
