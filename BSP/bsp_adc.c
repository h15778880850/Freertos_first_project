#include "bsp_adc.h"

#include "stm32f1xx.h"

#define BSP_ADC_TIMEOUT 100000U

bool BSP_Adc_Init(void)
{
  uint32_t timeout;

  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN;
  RCC->CFGR &= ~RCC_CFGR_ADCPRE;
  RCC->CFGR |= RCC_CFGR_ADCPRE_DIV6;

  GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);

  ADC1->CR1 = 0U;
  ADC1->CR2 = ADC_CR2_ADON;
  ADC1->SMPR2 |= ADC_SMPR2_SMP0;
  ADC1->SQR1 = 0U;
  ADC1->SQR3 = 0U;

  ADC1->CR2 |= ADC_CR2_RSTCAL;
  timeout = BSP_ADC_TIMEOUT;
  while (((ADC1->CR2 & ADC_CR2_RSTCAL) != 0U) && (timeout > 0U))
  {
    timeout--;
  }

  ADC1->CR2 |= ADC_CR2_CAL;
  timeout = BSP_ADC_TIMEOUT;
  while (((ADC1->CR2 & ADC_CR2_CAL) != 0U) && (timeout > 0U))
  {
    timeout--;
  }

  return timeout > 0U;
}

bool BSP_Adc_ReadRaw(uint16_t *value)
{
  uint32_t timeout = BSP_ADC_TIMEOUT;

  if (value == 0)
  {
    return false;
  }

  ADC1->SQR3 = 0U;
  ADC1->CR2 |= ADC_CR2_ADON;

  while (((ADC1->SR & ADC_SR_EOC) == 0U) && (timeout > 0U))
  {
    timeout--;
  }

  if (timeout == 0U)
  {
    return false;
  }

  *value = (uint16_t)(ADC1->DR & 0x0FFFU);
  return true;
}
