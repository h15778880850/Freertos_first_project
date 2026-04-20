#include "bsp_w25q64.h"

#include <string.h>

#include "main.h"

#define W25Q64_CS_PORT          GPIOA
#define W25Q64_CS_PIN           GPIO_PIN_4
#define W25Q64_TIMEOUT_MS       100U
#define W25Q64_PAGE_SIZE        256U
#define W25Q64_SECTOR_SIZE      4096U

#define W25Q64_CMD_WRITE_ENABLE 0x06U
#define W25Q64_CMD_READ_STATUS  0x05U
#define W25Q64_CMD_READ_DATA    0x03U
#define W25Q64_CMD_PAGE_PROGRAM 0x02U
#define W25Q64_CMD_SECTOR_ERASE 0x20U
#define W25Q64_CMD_JEDEC_ID     0x9FU

extern SPI_HandleTypeDef hspi1;

static bool s_flash_ready;

static void flash_cs(bool selected)
{
  HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, selected ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static bool flash_tx(const uint8_t *data, uint16_t length)
{
  return HAL_SPI_Transmit(&hspi1, (uint8_t *)data, length, W25Q64_TIMEOUT_MS) == HAL_OK;
}

static bool flash_rx(uint8_t *data, uint16_t length)
{
  return HAL_SPI_Receive(&hspi1, data, length, W25Q64_TIMEOUT_MS) == HAL_OK;
}

static bool flash_write_enable(void)
{
  uint8_t cmd = W25Q64_CMD_WRITE_ENABLE;

  flash_cs(true);
  bool ok = flash_tx(&cmd, 1U);
  flash_cs(false);

  return ok;
}

static bool flash_read_status(uint8_t *status)
{
  uint8_t cmd = W25Q64_CMD_READ_STATUS;
  bool ok;

  if (status == 0)
  {
    return false;
  }

  flash_cs(true);
  ok = flash_tx(&cmd, 1U) && flash_rx(status, 1U);
  flash_cs(false);

  return ok;
}

static bool flash_wait_ready(void)
{
  uint8_t status = 0xFFU;
  uint32_t started = HAL_GetTick();

  do
  {
    if (!flash_read_status(&status))
    {
      return false;
    }
  } while (((status & 0x01U) != 0U) && ((HAL_GetTick() - started) < 1000U));

  return (status & 0x01U) == 0U;
}

static bool flash_page_program(uint32_t address, const uint8_t *data, size_t length)
{
  uint8_t cmd[4];
  bool ok;

  if ((data == 0) || (length == 0U) || (length > W25Q64_PAGE_SIZE))
  {
    return false;
  }

  if (!flash_write_enable())
  {
    return false;
  }

  cmd[0] = W25Q64_CMD_PAGE_PROGRAM;
  cmd[1] = (uint8_t)(address >> 16);
  cmd[2] = (uint8_t)(address >> 8);
  cmd[3] = (uint8_t)address;

  flash_cs(true);
  ok = flash_tx(cmd, sizeof(cmd)) &&
       flash_tx(data, (uint16_t)length);
  flash_cs(false);

  return ok && flash_wait_ready();
}

bool BSP_W25Q64_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint32_t id;

  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = W25Q64_CS_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(W25Q64_CS_PORT, &GPIO_InitStruct);

  id = BSP_W25Q64_ReadJedecId();
  s_flash_ready = (id != 0x000000UL) && (id != 0xFFFFFFUL);
  return s_flash_ready;
}

uint32_t BSP_W25Q64_ReadJedecId(void)
{
  uint8_t cmd = W25Q64_CMD_JEDEC_ID;
  uint8_t id[3] = {0};
  bool ok;

  flash_cs(true);
  ok = flash_tx(&cmd, 1U) && flash_rx(id, sizeof(id));
  flash_cs(false);

  if (!ok)
  {
    return 0U;
  }

  return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

bool BSP_W25Q64_Read(uint32_t address, uint8_t *data, size_t length)
{
  uint8_t cmd[4];
  bool ok;

  if (!s_flash_ready || (data == 0) || (length > 65535U))
  {
    return false;
  }

  cmd[0] = W25Q64_CMD_READ_DATA;
  cmd[1] = (uint8_t)(address >> 16);
  cmd[2] = (uint8_t)(address >> 8);
  cmd[3] = (uint8_t)address;

  flash_cs(true);
  ok = flash_tx(cmd, sizeof(cmd)) &&
       flash_rx(data, (uint16_t)length);
  flash_cs(false);

  return ok;
}

bool BSP_W25Q64_WriteSector(uint32_t address, const uint8_t *data, size_t length)
{
  uint8_t cmd[4];
  bool ok;
  size_t offset = 0U;

  if (!s_flash_ready || (data == 0) || (length == 0U) || (length > W25Q64_SECTOR_SIZE))
  {
    return false;
  }

  address &= ~(W25Q64_SECTOR_SIZE - 1U);

  if (!flash_write_enable())
  {
    return false;
  }

  cmd[0] = W25Q64_CMD_SECTOR_ERASE;
  cmd[1] = (uint8_t)(address >> 16);
  cmd[2] = (uint8_t)(address >> 8);
  cmd[3] = (uint8_t)address;

  flash_cs(true);
  ok = flash_tx(cmd, sizeof(cmd));
  flash_cs(false);

  if (!ok || !flash_wait_ready())
  {
    return false;
  }

  while (offset < length)
  {
    size_t chunk = length - offset;
    if (chunk > W25Q64_PAGE_SIZE)
    {
      chunk = W25Q64_PAGE_SIZE;
    }

    if (!flash_page_program(address + offset, &data[offset], chunk))
    {
      return false;
    }

    offset += chunk;
  }

  return true;
}
