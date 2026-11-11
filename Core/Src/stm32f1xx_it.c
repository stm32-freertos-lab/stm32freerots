/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "main.h"
#include "stm32f1xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "driver_timer.h"
#include "rtos_app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
extern UART_HandleTypeDef huart1;
/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  NWatch_RtosFatal(NWATCH_FAULT_NMI, __FILE__, __LINE__);
}

/**
  * @brief This function handles Hard fault interrupt.
  */
/* HardFault_Handler 由 RTOS 移植层或故障捕获入口直接提供，禁止重复定义。 */

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  NWatch_RtosFatal(NWATCH_FAULT_MEMMANAGE, __FILE__, __LINE__);
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  NWatch_RtosFatal(NWATCH_FAULT_BUSFAULT, __FILE__, __LINE__);
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  NWatch_RtosFatal(NWATCH_FAULT_USAGEFAULT, __FILE__, __LINE__);
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
/* SVC_Handler 由 RTOS 移植层或故障捕获入口直接提供，禁止重复定义。 */

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
/* PendSV_Handler 由 RTOS 移植层或故障捕获入口直接提供，禁止重复定义。 */

/**
  * @brief This function handles System tick timer.
  */
/* SysTick_Handler 由 RTOS 移植层或故障捕获入口直接提供，禁止重复定义。 */

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
 void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/* USER CODE BEGIN 1 */
void TIM3_IRQHandler(void)
{
  MicroTimer_OverflowIRQHandler();
}

void TIM4_IRQHandler(void)
{
  Timebase_TickIRQHandler();
}


/* USER CODE END 1 */
