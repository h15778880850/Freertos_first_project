#include "bsp_w25q64.h"

#include <string.h>

#include "main.h"

#define W25Q64_CS_PORT          GPIOA
#define W25Q64_CS_PIN           GPIO_PIN_4
#define W25Q64_TIMEOUT_MS       100U
#define W25Q64_PAGE_SIZE        256U
#define W25Q64_SECTOR_SIZE      4096U
#define W25Q64_BUSY_TIMEOUT_MS  5000U

#define W25Q64_CMD_WRITE_ENABLE 0x06U
#define W25Q64_CMD_READ_STATUS  0x05U
#define W25Q64_CMD_READ_DATA    0x03U
#define W25Q64_CMD_PAGE_PROGRAM 0x02U
#define W25Q64_CMD_SECTOR_ERASE 0x20U
#define W25Q64_CMD_JEDEC_ID     0x9FU
#define W25Q64_CMD_RELEASE_PD   0xABU
#define W25Q64_CMD_ENABLE_RESET 0x66U
#define W25Q64_CMD_RESET_DEVICE 0x99U

extern SPI_HandleTypeDef hspi1;

static bool s_flash_ready;
static BspW25q64Error s_last_error = BSP_W25Q64_ERROR_NONE;

static bool flash_read_status(uint8_t *status);

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
  uint8_t tx = 0xFFU;

  for (uint16_t i = 0; i < length; i++)
  {
    if (HAL_SPI_TransmitReceive(&hspi1, &tx, &data[i], 1U, W25Q64_TIMEOUT_MS) != HAL_OK)
    {
      return false;
    }
  }

  return true;
}

static bool flash_cmd(uint8_t cmd)
{
  bool ok;

  flash_cs(true);
  ok = flash_tx(&cmd, 1U);
  flash_cs(false);

  return ok;
}

static bool flash_write_enable(void)
{
  uint8_t cmd = W25Q64_CMD_WRITE_ENABLE;
  uint8_t status = 0U;

  flash_cs(true);
  bool ok = flash_tx(&cmd, 1U);
  flash_cs(false);

  if (!ok)
  {
    return false;
  }

  if (!flash_read_status(&status))
  {
    return false;
  }

  return (status & 0x02U) != 0U;
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
  } while (((status & 0x01U) != 0U) && ((HAL_GetTick() - started) < W25Q64_BUSY_TIMEOUT_MS));

  return (status & 0x01U) == 0U;
}

static bool flash_page_program(uint32_t address, const uint8_t *data, size_t length)
{
  uint8_t cmd[4];
  bool ok;

  if ((data == 0) || (length == 0U) || (length > W25Q64_PAGE_SIZE))
  {
    s_last_error = BSP_W25Q64_ERROR_PARAM;
    return false;
  }

  if (!flash_write_enable())
  {
    s_last_error = BSP_W25Q64_ERROR_WRITE_ENABLE;
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

  if (!ok)
  {
    s_last_error = BSP_W25Q64_ERROR_PROGRAM;
    return false;
  }

  if (!flash_wait_ready())
  {
    s_last_error = BSP_W25Q64_ERROR_WAIT_READY;
    return false;
  }

  return true;
}

bool BSP_W25Q64_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint32_t id = 0U;

  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = W25Q64_CS_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(W25Q64_CS_PORT, &GPIO_InitStruct);

  (void)flash_cmd(W25Q64_CMD_ENABLE_RESET);
  (void)flash_cmd(W25Q64_CMD_RESET_DEVICE);
  HAL_Delay(2U);
  (void)flash_cmd(W25Q64_CMD_RELEASE_PD);
  HAL_Delay(1U);

  for (uint8_t retry = 0; retry < 5U; retry++)
  {
    id = BSP_W25Q64_ReadJedecId();
    if ((id != 0x000000UL) && (id != 0xFFFFFFUL))
    {
      break;
    }
    HAL_Delay(2U);
  }

  s_flash_ready = (id != 0x000000UL) && (id != 0xFFFFFFUL);
  s_last_error = s_flash_ready ? BSP_W25Q64_ERROR_NONE : BSP_W25Q64_ERROR_NOT_READY;
  return s_flash_ready;
}

bool BSP_W25Q64_IsReady(void)
{
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
    s_last_error = !s_flash_ready ? BSP_W25Q64_ERROR_NOT_READY : BSP_W25Q64_ERROR_PARAM;
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

  s_last_error = ok ? BSP_W25Q64_ERROR_NONE : BSP_W25Q64_ERROR_NOT_READY;
  return ok;
}

bool BSP_W25Q64_WriteSector(uint32_t address, const uint8_t *data, size_t length)
{
  uint8_t cmd[4];
  bool ok;
  size_t offset = 0U;

  if (!s_flash_ready && !BSP_W25Q64_Init())
  {
    s_last_error = BSP_W25Q64_ERROR_NOT_READY;
    return false;
  }

  if ((data == 0) || (length == 0U) || (length > W25Q64_SECTOR_SIZE))
  {
    s_last_error = BSP_W25Q64_ERROR_PARAM;
    return false;
  }

  address &= ~(W25Q64_SECTOR_SIZE - 1U);

  if (!flash_write_enable())
  {
    s_last_error = BSP_W25Q64_ERROR_WRITE_ENABLE;
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
    s_last_error = ok ? BSP_W25Q64_ERROR_WAIT_READY : BSP_W25Q64_ERROR_ERASE;
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
      if (s_last_error == BSP_W25Q64_ERROR_NONE)
      {
        s_last_error = BSP_W25Q64_ERROR_PROGRAM;
      }
      return false;
    }

    offset += chunk;
  }

  s_last_error = BSP_W25Q64_ERROR_NONE;
  return true;
}

uint8_t BSP_W25Q64_ReadStatusReg(void)
{
  uint8_t status = 0xFFU;

  if (!flash_read_status(&status))
  {
    s_last_error = BSP_W25Q64_ERROR_NOT_READY;
    return 0xFFU;
  }

  return status;
}

BspW25q64Error BSP_W25Q64_GetLastError(void)
{
  return s_last_error;
}
