#include "bsp_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "stm32f1xx.h"

#define BSP_LOG_BAUDRATE 115200U
#define BSP_LOG_TIMEOUT  100000U

static bool s_log_ready;

static void log_write_char(char ch)
{
  uint32_t timeout = BSP_LOG_TIMEOUT;

  while (((USART1->SR & USART_SR_TXE) == 0U) && (timeout > 0U))
  {
    timeout--;
  }

  if (timeout > 0U)
  {
    USART1->DR = (uint16_t)ch;
  }
}

bool BSP_Log_Init(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;

  GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9 | GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
  GPIOA->CRH |= GPIO_CRH_MODE9_0 | GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1 | GPIO_CRH_CNF10_0;

  USART1->CR1 = 0U;
  USART1->BRR = (uint16_t)((SystemCoreClock + (BSP_LOG_BAUDRATE / 2U)) / BSP_LOG_BAUDRATE);
  USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

  s_log_ready = true;
  return true;
}

void BSP_Log_Print(const char *fmt, ...)
{
  char buffer[128];
  va_list args;
  int length;

  if (!s_log_ready)
  {
    return;
  }

  va_start(args, fmt);
  length = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (length < 0)
  {
    return;
  }

  if ((size_t)length >= sizeof(buffer))
  {
    length = (int)sizeof(buffer) - 1;
  }

  for (int i = 0; i < length; i++)
  {
    if (buffer[i] == '\n')
    {
      log_write_char('\r');
    }
    log_write_char(buffer[i]);
  }
}
