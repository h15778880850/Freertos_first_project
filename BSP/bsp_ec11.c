#include "bsp_ec11.h"

#include "cmsis_os.h"
#include "main.h"

#define EC11_S1_PORT          GPIOB
#define EC11_S1_PIN           GPIO_PIN_12
#define EC11_S2_PORT          GPIOB
#define EC11_S2_PIN           GPIO_PIN_0
#define EC11_KEY_PORT         GPIOB
#define EC11_KEY_PIN          GPIO_PIN_1
#define EC11_KEY_DEBOUNCE_MS  50U

static volatile int32_t s_ec11_rotation;
static osThreadId_t s_ui_task_handle;

void BSP_EC11_Init(void)
{
  s_ec11_rotation = 0;
  s_ui_task_handle = NULL;
}

void BSP_EC11_SetTaskHandle(osThreadId_t handle)
{
  s_ui_task_handle = handle;
}

void BSP_EC11_HandleS1Isr(void)
{
  if ((EC11_S1_PORT->IDR & EC11_S1_PIN) == 0U)
  {
    return;
  }

  if ((EC11_S2_PORT->IDR & EC11_S2_PIN) == 0U)
  {
    s_ec11_rotation++;
  }
  else
  {
    s_ec11_rotation--;
  }

  if (s_ui_task_handle != NULL)
  {
    (void)osThreadFlagsSet(s_ui_task_handle, EC11_FLAG_ROTATION);
  }
}

int32_t BSP_EC11_GetRotation(void)
{
  int32_t result;

  __disable_irq();
  result = s_ec11_rotation;
  s_ec11_rotation = 0;
  __enable_irq();

  return result;
}

bool BSP_EC11_IsKeyPressed(void)
{
  static uint32_t last_release_tick;
  static bool pressed_seen;
  uint32_t now = HAL_GetTick();
  bool pin_low = (EC11_KEY_PORT->IDR & EC11_KEY_PIN) == 0U;

  if (pin_low)
  {
    pressed_seen = true;
  }

  if (pressed_seen && !pin_low)
  {
    if ((now - last_release_tick) >= EC11_KEY_DEBOUNCE_MS)
    {
      last_release_tick = now;
      pressed_seen = false;
      return true;
    }
  }

  return false;
}
