#ifndef NWATCH_FREERTOS_CONFIG_H
#define NWATCH_FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;
void NWatch_RtosAssert(const char *file, uint32_t line);

/* 单核 Cortex-M3；GUI 继续使用 TIM4 毫秒时基，SysTick 只服务内核。 */
#define configNUMBER_OF_CORES                   1
#define configCPU_CLOCK_HZ                      SystemCoreClock
#define configTICK_RATE_HZ                      1000U
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configMAX_PRIORITIES                    4
#define configMAX_TASK_NAME_LEN                 12
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TICKLESS_IDLE                 0

/* 单位为字：Idle 128*4B。GUI 的 768*4B 从同一个 heap_4 堆分配。 */
#define configMINIMAL_STACK_SIZE         128U
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION  0
#define configTOTAL_HEAP_SIZE            (10U * 1024U)
#define configAPPLICATION_ALLOCATED_HEAP 0
#define configUSE_MALLOC_FAILED_HOOK     1
#define configCHECK_FOR_STACK_OVERFLOW   2
#define configUSE_IDLE_HOOK              1
#define configUSE_TICK_HOOK              1

/* 不创建软件定时器任务，也不启用本轮不需要的同步功能。 */
#define configUSE_TIMERS                     1
#define configTIMER_TASK_PRIORITY            2
#define configTIMER_QUEUE_LENGTH             5
#define configTIMER_TASK_STACK_DEPTH         192
#define configUSE_CO_ROUTINES                0
#define configUSE_MUTEXES                    0
#define configUSE_RECURSIVE_MUTEXES          0
#define configUSE_COUNTING_SEMAPHORES        0
#define configUSE_TASK_NOTIFICATIONS         1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    2
#define configQUEUE_REGISTRY_SIZE            0
#define configUSE_TRACE_FACILITY             0
#define configUSE_STATS_FORMATTING_FUNCTIONS 0
#define configGENERATE_RUN_TIME_STATS        0
#define configUSE_NEWLIB_REENTRANT           0
#define configENABLE_BACKWARD_COMPATIBILITY  0

#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskDelayUntil              1
#define INCLUDE_vTaskDelete                  0
#define INCLUDE_vTaskSuspend                 0
#define INCLUDE_vTaskPrioritySet             0
#define INCLUDE_uxTaskPriorityGet            0
#define INCLUDE_xTaskGetSchedulerState       1
#define INCLUDE_xTaskGetIdleTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark  1

/* F103 有 4 个优先级位：TIM3/4=4，EXTI=5，PendSV/SysTick=15。 */
#define configPRIO_BITS                              4U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
/* RVDS 汇编会直接展开此常量，使用无 C 类型后缀的立即数。 */
#define configKERNEL_INTERRUPT_PRIORITY      0xF0
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 0x50
#if configMAX_SYSCALL_INTERRUPT_PRIORITY != (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#error FreeRTOS interrupt priority encoding mismatch
#endif
#define configASSERT(x)                                  \
    do {                                                 \
        if (!(x)) NWatch_RtosAssert(__FILE__, __LINE__); \
    } while (0)

/* 直接由 RVDS 裸异常入口提供向量符号，禁止添加普通 C 包装函数。 */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif
