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
  uint16_t raw;
  uint16_t threshold;
  bool alarm_active;
} AppSample;

typedef struct
{
  bool uart_ok;
  bool adc_ok;
  bool oled_ok;
  bool flash_ok;
  bool config_restored;
} AppSelfTest;

#endif
