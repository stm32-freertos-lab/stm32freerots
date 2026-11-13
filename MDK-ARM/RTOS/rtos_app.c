#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "rtos_app.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_it.h"
#include "driver_timer.h"
#include "sensor.h"
#include "env_event.h" /* 引用环境事件组句柄和任务声明。 */

volatile NWatchRtosDiagnostics g_nwatchRtos;
volatile NWatchRtosFault g_nwatchFault;
static TaskHandle_t guiHandle;
QueueHandle_t g_sensorQueue = NULL;
TaskHandle_t sensorHandle = NULL;
TaskHandle_t rgbHandle = NULL;
TimerHandle_t muteTimer = NULL;

static void RGB_TimerCallback(TimerHandle_t muteTimer) /* 周期到期后通知 RGB 任务，不直接操作灯。 */
{
    (void)muteTimer; /* 本回调不使用定时器句柄参数。 */

    if (rgbHandle != NULL)
    {
        xTaskNotifyGiveIndexed(rgbHandle, 1U); /* 向 RGB 的第 1 号通知槽发送计数通知。 */
    }
}

static void updateDiagnostics(void) /* 更新任务数量、栈水位和堆余量等运行诊断。 */
{
    /* 与其他诊断同频采样，调试器无需调用内核函数。 */
    g_nwatchRtos.schedulerState = (uint32_t)xTaskGetSchedulerState();
    g_nwatchRtos.kernelTickCount = xTaskGetTickCount();
    g_nwatchRtos.taskCount = uxTaskGetNumberOfTasks();
    g_nwatchRtos.guiStackHighWaterWords = uxTaskGetStackHighWaterMark(guiHandle);
    g_nwatchRtos.idleStackHighWaterWords = uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
    g_nwatchRtos.freeHeapBytes = xPortGetFreeHeapSize();
    g_nwatchRtos.minEverFreeHeapBytes = xPortGetMinimumEverFreeHeapSize();
    g_nwatchRtos.halMilliseconds = HAL_GetTick();
    g_nwatchRtos.microsecondsLow = (uint32_t)system_get_us();
    g_nwatchRtos.controlRegister = __get_CONTROL();
    g_nwatchRtos.processStackPointer = __get_PSP();
    g_nwatchRtos.mainStackPointer = __get_MSP();
    g_nwatchRtos.basepri = __get_BASEPRI();
    g_nwatchRtos.systickReload = SysTick->LOAD;
    g_nwatchRtos.systickControl = SysTick->CTRL;
}

void NWatch_RtosStart(void (*guiTask)(void *)) /* 检查中断配置，创建 GUI 并启动调度器。 */
{
    const uint32_t *vectors = (const uint32_t *)SCB->VTOR;
    configASSERT(SystemCoreClock == 72000000U);
    configASSERT(HAL_NVIC_GetPriorityGrouping() == NVIC_PRIORITYGROUP_4);
    configASSERT(NVIC_GetPriority(TIM3_IRQn) == 4U);
    configASSERT(NVIC_GetPriority(TIM4_IRQn) == 4U);
    configASSERT(NVIC_GetPriority(EXTI15_10_IRQn) == 5U);
    configASSERT(vectors[11] == (uint32_t)SVC_Handler);
    configASSERT(vectors[14] == (uint32_t)PendSV_Handler);
    configASSERT(vectors[15] == (uint32_t)SysTick_Handler);

    g_nwatchRtos.magic = 0x4E575254U;
    g_nwatchRtos.phase = 1U;
    g_nwatchRtos.guiStackWords = NWATCH_GUI_STACK_WORDS; 
    g_nwatchRtos.idleStackWords = configMINIMAL_STACK_SIZE;   
    g_nwatchRtos.heapTotalBytes = configTOTAL_HEAP_SIZE;
    /* GUI 和内核 Idle 的栈/TCB 都从 heap_4 分配，不另建重复栈数组。 */
    /* 保留实际创建返回值，便于 ST-Link 区分未启动和创建失败。 */
    g_nwatchRtos.taskCreateResult = xTaskCreate(guiTask, "NWatchGUI", NWATCH_GUI_STACK_WORDS,
                                              NULL, NWATCH_GUI_PRIORITY, &guiHandle);
    if(g_nwatchRtos.taskCreateResult != pdPASS)
        NWatch_RtosFatal(NWATCH_FAULT_TASK_CREATE, __FILE__, __LINE__);

    g_nwatchRtos.freeHeapBytes = xPortGetFreeHeapSize();
    g_nwatchRtos.minEverFreeHeapBytes = xPortGetMinimumEverFreeHeapSize();
    vTaskStartScheduler();
    /* 调度器返回通常代表 Idle 创建失败，不能继续跑裸机主循环。 */
    NWatch_RtosFatal(NWATCH_FAULT_SCHEDULER_RETURN, __FILE__, __LINE__);
}

void NWatch_RtosGuiEntered(void) /* GUI 首次运行时检查任务上下文并记录启动阶段。 */
{
    /* 首个 SVC 返回后应在 Thread/PSP 上下文运行。 */
    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    configASSERT(__get_IPSR() == 0U);
    configASSERT((__get_CONTROL() & 2U) != 0U);
    configASSERT(__get_BASEPRI() == 0U);
    configASSERT(NVIC_GetPriority(PendSV_IRQn) == configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    configASSERT(NVIC_GetPriority(SysTick_IRQn) == configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    configASSERT(SysTick->LOAD == SystemCoreClock / configTICK_RATE_HZ - 1U);
    g_nwatchRtos.phase = 2U;
    updateDiagnostics();
    configASSERT(g_nwatchRtos.taskCount >= 2U); /* 至少包含 GUI 和内核 Idle。 */
}

/* GUI 初始化完成后的入口：记录就绪状态，再创建传感器任务。 */
void NWatch_RtosGuiReady(void) /* GUI 初始化后创建通信对象、工作任务和周期定时器。 */
{
    g_sensorQueue = xQueueCreate(1, sizeof(sensor_data_t));
    /* 在环境任务创建前分配事件组，初始事件位均为 0。 */
    g_envEventGroup = xEventGroupCreate();
    if(g_sensorQueue == NULL)
    {
        /* 队列分配失败，复用已有的内存分配故障码。 */
        NWatch_RtosFatal(NWATCH_FAULT_MALLOC, __FILE__, __LINE__);
    }

    if (xTaskCreate(RGB_Task, "RGB", 192,
                    NULL, 1, &rgbHandle) != pdPASS)
    {
        NWatch_RtosFatal(NWATCH_FAULT_TASK_CREATE,
                         __FILE__, __LINE__);
    }

    /* 保留原有失败处理，仅在事件组有效时创建环境任务。 */
    if (g_envEventGroup == NULL)
    {
        NWatch_RtosFatal(NWATCH_FAULT_MALLOC, __FILE__, __LINE__);
    }
    if (xTaskCreate(Env_Task, "Environment", 192, NULL, 1, NULL) != pdPASS)
    {
        NWatch_RtosFatal(NWATCH_FAULT_TASK_CREATE, __FILE__, __LINE__);
    }
    muteTimer = xTimerCreate("Mute", pdMS_TO_TICKS(30000), pdTRUE,
                            NULL, RGB_TimerCallback); /* 创建每 30 秒到期的自动重载定时器，保留原句柄名。 */

   if (muteTimer == NULL)
   {
       NWatch_RtosFatal(NWATCH_FAULT_MALLOC, __FILE__, __LINE__);
   }
   if (xTimerStart(muteTimer, 0) != pdPASS) /* 非阻塞提交启动命令，保留初始化时启动的逻辑。 */
   {
       // 启动命令提交失败，需要处理
   }
    /* phase=3 表示 GUI 初始化完成；先采样一次创建前的诊断数据。 */
    g_nwatchRtos.phase = 3U;
    updateDiagnostics();
    /* 创建 Sensor_Task，各参数含义如下：
     * Sensor_Task：任务入口函数；"Sensor"：供调试识别的任务名称。
     * 192：栈深度，单位为 32 位字，本工程对应 768 字节，仅指任务栈。
     * 第一个 NULL：不向任务入口传入额外参数。
     * 3：任务优先级，高于 GUI 的 2；创建成功后可能在本函数返回前抢占 GUI。
     * 最后一个 NULL：不保存任务句柄；栈和任务控制块仍由内核管理。
     * pdPASS 表示创建成功；返回其他值时进入下面的故障处理。
     */
if(xTaskCreate(Sensor_Task, "Sensor", 192, guiHandle, 3, &sensorHandle) != pdPASS)
    {
        /* 记录任务创建失败的位置，并进入已有的故障停机流程。 */
        NWatch_RtosFatal(NWATCH_FAULT_TASK_CREATE, __FILE__, __LINE__);
    }

    /* 记录创建后的任务数和 Heap 余量。 */
    updateDiagnostics();
}

void NWatch_RtosPoll(void) /* 记录 GUI 循环耗时，每秒刷新运行诊断。 */
{
    static TickType_t lastSample;
    static uint32_t lastPollUs;
    TickType_t now = xTaskGetTickCount();
    uint32_t nowUs = (uint32_t)system_get_us();
    if(g_nwatchRtos.guiIterations != 0U)
    {
        g_nwatchRtos.guiLoopLastUs = nowUs - lastPollUs;
        if(g_nwatchRtos.guiLoopLastUs > g_nwatchRtos.guiLoopMaxUs)
            g_nwatchRtos.guiLoopMaxUs = g_nwatchRtos.guiLoopLastUs;
    }
    lastPollUs = nowUs;
    ++g_nwatchRtos.guiIterations;
    /* 每秒采样一次栈水位和堆余量，避免干扰 OLED 帧率。 */
    if((TickType_t)(now - lastSample) >= pdMS_TO_TICKS(1000U))
    {
        lastSample = now;
        updateDiagnostics();
    }
}

void vApplicationTickHook(void) /* 内核 Tick 中断钩子，只累计计数。 */
{
    /* ISR 只记录计数，不调用 HAL_IncTick、GUI 或其他 FreeRTOS API。 */
    ++g_nwatchRtos.tickHookCount;
}

void vApplicationIdleHook(void) /* 空闲任务钩子，等待中断以降低空闲功耗。 */
{
    ++g_nwatchRtos.idleHookCount;
    /* 普通 WFI 不停止 TIM3/TIM4，未启用 tickless 或 STOP。 */
    __WFI();
}

void NWatch_RtosFatal(uint32_t reason, const char *file, uint32_t line) /* 保存故障现场并停止运行。 */
{
    __disable_irq();
    g_nwatchFault.reason = reason;
    g_nwatchFault.file = file;
    g_nwatchFault.line = line;
    g_nwatchFault.exceptionNumber = __get_IPSR();
    g_nwatchFault.cfsr = SCB->CFSR;
    g_nwatchFault.hfsr = SCB->HFSR;
    g_nwatchFault.mmfar = SCB->MMFAR;
    g_nwatchFault.bfar = SCB->BFAR;
    g_nwatchFault.msp = __get_MSP();
    g_nwatchFault.psp = __get_PSP();
    /* 不在故障路径调用 printf/OLED/分配内存；连接调试器时主动停住。 */
    if((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U)
        __BKPT(0);
    for(;;)
        __NOP();
}

void NWatch_RtosAssert(const char *file, uint32_t line) /* 将断言失败交给统一故障处理。 */
{
    NWatch_RtosFatal(NWATCH_FAULT_ASSERT, file, line);
}

void vApplicationMallocFailedHook(void) /* RTOS 动态内存分配失败时记录故障。 */
{
    NWatch_RtosFatal(NWATCH_FAULT_MALLOC, __FILE__, __LINE__);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *taskName) /* 记录发生栈溢出的任务并停机。 */
{
    g_nwatchFault.taskHandle = (uint32_t)task;
    g_nwatchFault.taskName = taskName;
    NWatch_RtosFatal(NWATCH_FAULT_STACK, __FILE__, __LINE__);
}

__attribute__((noreturn)) void NWatch_HardFaultCapture(uint32_t *frame, uint32_t exceptionReturn) /* 检查异常栈并保存故障 PC 等信息。 */
{
    uint32_t address = (uint32_t)frame;
    g_nwatchFault.exceptionReturn = exceptionReturn;
    /* 栈入栈出错时不解引用失效 PSP；范围始终按真实 20KB RAM 检查。 */
    if((SCB->CFSR & ((1U << 4) | (1U << 12))) == 0U &&
       (address & 3U) == 0U && address >= 0x20000000U && address <= 0x20004FE0U)
    {
        g_nwatchFault.frameValid = 1U;
        g_nwatchFault.stackedLR = frame[5];
        g_nwatchFault.stackedPC = frame[6];
        g_nwatchFault.stackedXPSR = frame[7];
    }
    NWatch_RtosFatal(NWATCH_FAULT_HARDFAULT, __FILE__, __LINE__);
}

/* 直接接收异常 LR，先判断 MSP/PSP，再使用独立主栈记录故障。 */
__asm void HardFault_Handler(void) /* 选择故障现场所在栈，转交 C 函数记录异常。 */
{
    EXTERN NWatch_HardFaultCapture
    TST lr, #4
    ITE EQ
    MRSEQ r0, MSP
    MRSNE r0, PSP
    MOV r1, lr
    B NWatch_HardFaultCapture
}
