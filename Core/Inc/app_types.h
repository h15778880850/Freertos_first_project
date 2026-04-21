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

typedef enum
{
  APP_SAMPLE_SOURCE_DS18B20 = 0,
  APP_SAMPLE_SOURCE_ADC = 1
} AppSampleSource;

typedef struct
{
  uint16_t raw;
  int16_t temperature_centi;
  uint16_t threshold;
  AppSampleSource source;
  bool alarm_active;
  bool k1_triggered;
} AppSample;

typedef struct
{
  bool uart_ok;
  bool adc_ok;
  bool ds18b20_ok;
  bool oled_ok;
  bool flash_ok;
  bool config_restored;
} AppSelfTest;

#endif
