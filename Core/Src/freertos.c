/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_main.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osMessageQueueId_t sample_queue;
osMessageQueueId_t log_queue;
osMessageQueueId_t storage_queue;
osMutexId_t config_mutex;
osTimerId_t save_timer;

osThreadId_t sensor_task_handle;
osThreadId_t ui_task_handle;
osThreadId_t storage_task_handle;
osThreadId_t log_task_handle;

const osThreadAttr_t sensor_task_attributes = {
  .name = "sensor_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t ui_task_attributes = {
  .name = "ui_task",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

const osThreadAttr_t storage_task_attributes = {
  .name = "storage_task",
  .stack_size = 320 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

const osThreadAttr_t log_task_attributes = {
  .name = "log_task",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void MX_FREERTOS_Init(void)
{
  AppRtosObjects app_objects;

  sample_queue = osMessageQueueNew(4U, sizeof(AppSample), NULL);
  log_queue = osMessageQueueNew(12U, sizeof(AppLogMessage), NULL);
  storage_queue = osMessageQueueNew(2U, sizeof(AppConfig), NULL);
  config_mutex = osMutexNew(NULL);
  save_timer = osTimerNew(save_timer_callback, osTimerOnce, NULL, NULL);

  if ((sample_queue == NULL) ||
      (log_queue == NULL) ||
      (storage_queue == NULL) ||
      (config_mutex == NULL) ||
      (save_timer == NULL))
  {
    Error_Handler();
  }

  app_objects.sample_queue = sample_queue;
  app_objects.log_queue = log_queue;
  app_objects.storage_queue = storage_queue;
  app_objects.config_mutex = config_mutex;
  app_objects.save_timer = save_timer;
  App_Main(&app_objects);

  log_task_handle = osThreadNew(log_task, NULL, &log_task_attributes);
  storage_task_handle = osThreadNew(storage_task, NULL, &storage_task_attributes);
  ui_task_handle = osThreadNew(ui_task, NULL, &ui_task_attributes);
  sensor_task_handle = osThreadNew(sensor_task, NULL, &sensor_task_attributes);

  if ((log_task_handle == NULL) ||
      (storage_task_handle == NULL) ||
      (ui_task_handle == NULL) ||
      (sensor_task_handle == NULL))
  {
    Error_Handler();
  }
}

/* USER CODE END Application */

