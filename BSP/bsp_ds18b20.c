#include "bsp_ds18b20.h"

#include "main.h"

#define DS18B20_PORT GPIOA
#define DS18B20_PIN  GPIO_PIN_1

#define DS18B20_CMD_SKIP_ROM      0xCCU
#define DS18B20_CMD_CONVERT_T     0x44U
#define DS18B20_CMD_READ_SCRATCH  0xBEU

static void ds18b20_delay_us(uint32_t us)
{
  uint32_t loops = (SystemCoreClock / 4000000U) * us;

  if (loops == 0U)
  {
    loops = us;
  }

  while (loops-- > 0U)
  {
    __NOP();
  }
}

static void ds18b20_pin_output(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = DS18B20_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

static void ds18b20_pin_input(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Pin = DS18B20_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

static bool ds18b20_reset(void)
{
  bool present;

  __disable_irq();
  ds18b20_pin_output();
  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
  ds18b20_delay_us(480U);
  ds18b20_pin_input();
  ds18b20_delay_us(70U);
  present = HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN) == GPIO_PIN_RESET;
  ds18b20_delay_us(410U);
  __enable_irq();

  return present;
}

static void ds18b20_write_bit(uint8_t bit)
{
  __disable_irq();
  ds18b20_pin_output();
  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);

  if (bit != 0U)
  {
    ds18b20_delay_us(6U);
    ds18b20_pin_input();
    ds18b20_delay_us(64U);
  }
  else
  {
    ds18b20_delay_us(60U);
    ds18b20_pin_input();
    ds18b20_delay_us(10U);
  }

  __enable_irq();
}

static uint8_t ds18b20_read_bit(void)
{
  uint8_t bit;

  __disable_irq();
  ds18b20_pin_output();
  HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
  ds18b20_delay_us(6U);
  ds18b20_pin_input();
  ds18b20_delay_us(9U);
  bit = (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN) == GPIO_PIN_SET) ? 1U : 0U;
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

bool BSP_DS18B20_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  ds18b20_pin_input();
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

  if (ds18b20_crc8(scratchpad, 8U) != scratchpad[8])
  {
    return false;
  }

  raw = (int16_t)(((uint16_t)scratchpad[1] << 8) | scratchpad[0]);
  centi = ((int32_t)raw * 100L) / 16L;
  *temperature_centi = (int16_t)centi;

  return true;
}
