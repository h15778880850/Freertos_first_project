#include "bsp_rgb.h"

#include "main.h"

#define RGB_RED_PORT    GPIOC
#define RGB_RED_PIN     GPIO_PIN_13
#define RGB_GREEN_PORT  GPIOA
#define RGB_GREEN_PIN   GPIO_PIN_1
#define RGB_BLUE_PORT   GPIOB
#define RGB_BLUE_PIN    GPIO_PIN_9

static void rgb_write(GPIO_TypeDef *port, uint16_t pin, bool on)
{
  HAL_GPIO_WritePin(port, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BSP_Rgb_Init(void)
{
  rgb_write(RGB_RED_PORT, RGB_RED_PIN, false);
  rgb_write(RGB_GREEN_PORT, RGB_GREEN_PIN, false);
  rgb_write(RGB_BLUE_PORT, RGB_BLUE_PIN, false);
}

void BSP_Rgb_SetAlarm(bool active)
{
  rgb_write(RGB_RED_PORT, RGB_RED_PIN, active);
  rgb_write(RGB_GREEN_PORT, RGB_GREEN_PIN, !active);
  rgb_write(RGB_BLUE_PORT, RGB_BLUE_PIN, false);
}

void BSP_Rgb_SetSelfTest(bool ok)
{
  rgb_write(RGB_RED_PORT, RGB_RED_PIN, !ok);
  rgb_write(RGB_GREEN_PORT, RGB_GREEN_PIN, ok);
  rgb_write(RGB_BLUE_PORT, RGB_BLUE_PIN, true);
}
