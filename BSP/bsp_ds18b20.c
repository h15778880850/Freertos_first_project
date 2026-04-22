#include "bsp_ds18b20.h"

#include "main.h"

#define DS18B20_PORT GPIOA
#define DS18B20_PIN  GPIO_PIN_1

#define DS18B20_CMD_SKIP_ROM      0xCCU
#define DS18B20_CMD_CONVERT_T     0x44U
#define DS18B20_CMD_READ_SCRATCH  0xBEU

static void ds18b20_delay_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void ds18b20_delay_us(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = (SystemCoreClock / 1000000U) * us;

  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
  {
    ds18b20_delay_init();
    start = DWT->CYCCNT;
  }

  while ((DWT->CYCCNT - start) < ticks)
  {
    __NOP();
  }
}

static void ds18b20_pin_init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();

  DS18B20_PORT->BSRR = DS18B20_PIN;
  DS18B20_PORT->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
  DS18B20_PORT->CRL |= GPIO_CRL_MODE1_1 | GPIO_CRL_CNF1_0;
}

static void ds18b20_drive_low(void)
{
  DS18B20_PORT->BRR = DS18B20_PIN;
}

static void ds18b20_release(void)
{
  DS18B20_PORT->BSRR = DS18B20_PIN;
}

static bool ds18b20_read_pin(void)
{
  return (DS18B20_PORT->IDR & DS18B20_PIN) != 0U;
}

static bool ds18b20_reset(void)
{
  bool present;

  __disable_irq();
  ds18b20_drive_low();
  ds18b20_delay_us(480U);
  ds18b20_release();
  ds18b20_delay_us(70U);
  present = !ds18b20_read_pin();
  ds18b20_delay_us(410U);
  __enable_irq();

  return present;
}

static void ds18b20_write_bit(uint8_t bit)
{
  __disable_irq();
  ds18b20_drive_low();

  if (bit != 0U)
  {
    ds18b20_delay_us(6U);
    ds18b20_release();
    ds18b20_delay_us(64U);
  }
  else
  {
    ds18b20_delay_us(60U);
    ds18b20_release();
    ds18b20_delay_us(10U);
  }

  __enable_irq();
}

static uint8_t ds18b20_read_bit(void)
{
  uint8_t bit;

  __disable_irq();
  ds18b20_drive_low();
  ds18b20_delay_us(6U);
  ds18b20_release();
  ds18b20_delay_us(9U);
  bit = ds18b20_read_pin() ? 1U : 0U;
  ds18b20_delay_us(55U);
  __enable_irq();

  return bit;
}

static void ds18b20_write_byte(uint8_t value)
{
  for (uint8_t i = 0; i < 8U; i++)
  {
    ds18b20_write_bit((uint8_t)(value & 0x01U));
    value >>= 1;
  }
}

static uint8_t ds18b20_read_byte(void)
{
  uint8_t value = 0U;

  for (uint8_t i = 0; i < 8U; i++)
  {
    value |= (uint8_t)(ds18b20_read_bit() << i);
  }

  return value;
}

static uint8_t ds18b20_crc8(const uint8_t *data, uint8_t length)
{
  uint8_t crc = 0U;

  for (uint8_t i = 0; i < length; i++)
  {
    uint8_t inbyte = data[i];
    for (uint8_t j = 0; j < 8U; j++)
    {
      uint8_t mix = (uint8_t)((crc ^ inbyte) & 0x01U);
      crc >>= 1;
      if (mix != 0U)
      {
        crc ^= 0x8CU;
      }
      inbyte >>= 1;
    }
  }

  return crc;
}

static bool ds18b20_scratchpad_plausible(const uint8_t *scratchpad)
{
  bool all_zero = true;
  bool all_ff = true;

  for (uint8_t i = 0; i < 9U; i++)
  {
    if (scratchpad[i] != 0x00U)
    {
      all_zero = false;
    }

    if (scratchpad[i] != 0xFFU)
    {
      all_ff = false;
    }
  }

  return !all_zero && !all_ff;
}

bool BSP_DS18B20_Init(void)
{
  ds18b20_delay_init();
  ds18b20_pin_init();
  ds18b20_delay_us(1000U);
  return ds18b20_reset();
}

bool BSP_DS18B20_StartConversion(void)
{
  if (!ds18b20_reset())
  {
    return false;
  }

  ds18b20_write_byte(DS18B20_CMD_SKIP_ROM);
  ds18b20_write_byte(DS18B20_CMD_CONVERT_T);
  return true;
}

bool BSP_DS18B20_ReadTemperatureCenti(int16_t *temperature_centi)
{
  uint8_t scratchpad[9];
  int16_t raw;
  int32_t centi;

  if (temperature_centi == 0)
  {
    return false;
  }

  if (!ds18b20_reset())
  {
    return false;
  }

  ds18b20_write_byte(DS18B20_CMD_SKIP_ROM);
  ds18b20_write_byte(DS18B20_CMD_READ_SCRATCH);

  for (uint8_t i = 0; i < sizeof(scratchpad); i++)
  {
    scratchpad[i] = ds18b20_read_byte();
  }

  if ((ds18b20_crc8(scratchpad, 8U) != scratchpad[8]) &&
      !ds18b20_scratchpad_plausible(scratchpad))
  {
    return false;
  }

  raw = (int16_t)(((uint16_t)scratchpad[1] << 8) | scratchpad[0]);
  centi = ((int32_t)raw * 100L) / 16L;
  *temperature_centi = (int16_t)centi;

  return true;
}
