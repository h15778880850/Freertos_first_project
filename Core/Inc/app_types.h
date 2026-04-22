#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t sample_period_ms;
  uint16_t alarm_threshold;
  uint16_t alarm_hysteresis;
} AppConfig;

typedef struct
{
  int16_t temperature_centi;
  uint16_t threshold;
  bool alarm_active;
  bool k1_triggered;
  bool valid;
} AppSample;

typedef struct
{
  bool uart_ok;
  bool ds18b20_ok;
  bool oled_ok;
  bool flash_ok;
  bool config_restored;
  uint32_t flash_jedec_id;
} AppSelfTest;

#endif
