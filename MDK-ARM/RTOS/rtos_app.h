#ifndef NWATCH_RTOS_APP_H
#define NWATCH_RTOS_APP_H

#include <stdint.h>

/* 仅用于分步构建验证；正式 GUI 镜像必须为 0。 */
#ifndef NWATCH_RTOS_BRINGUP_ONLY
#define NWATCH_RTOS_BRINGUP_ONLY 0
#endif
#define NWATCH_GUI_STACK_WORDS 768U
#define NWATCH_GUI_PRIORITY    2U

enum {
    NWATCH_FAULT_NONE = 0,
    NWATCH_FAULT_ASSERT,
    NWATCH_FAULT_MALLOC,
    NWATCH_FAULT_STACK,
    NWATCH_FAULT_TASK_CREATE,
    NWATCH_FAULT_SCHEDULER_RETURN,
    NWATCH_FAULT_HARDFAULT,
    NWATCH_FAULT_MEMMANAGE,
    NWATCH_FAULT_BUSFAULT,
    NWATCH_FAULT_USAGEFAULT,
    NWATCH_FAULT_NMI,
    NWATCH_FAULT_HAL
};

/* 用调试器观察；栈水位单位为 32 位字，堆空间单位为字节。 */
typedef struct {
    uint32_t magic;
    uint32_t phase;              /* 1=准备调度，2=GUI 已进入，3=UI 初始化完成 */
    uint32_t guiIterations;
    uint32_t kernelTickCount;
    uint32_t tickHookCount;
    uint32_t idleHookCount;
    uint32_t taskCount;
    uint32_t guiStackWords;
    uint32_t idleStackWords;
    uint32_t guiStackHighWaterWords;
    uint32_t idleStackHighWaterWords;
    uint32_t heapTotalBytes;
    uint32_t freeHeapBytes;
    uint32_t minEverFreeHeapBytes;
    uint32_t halMilliseconds;
    uint32_t microsecondsLow;
    uint32_t controlRegister;
    uint32_t processStackPointer;
    uint32_t mainStackPointer;
    uint32_t basepri;
    uint32_t systickReload;
    uint32_t systickControl;
    uint32_t schedulerState;     /* 2=taskSCHEDULER_RUNNING，每秒更新。 */
    int32_t taskCreateResult;     /* 1=pdPASS；失败时由故障钩子记录。 */
} NWatchRtosDiagnostics;

typedef struct {
    uint32_t reason;
    const char *file;
    uint32_t line;
    const char *taskName;
    uint32_t taskHandle;
    uint32_t exceptionNumber;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t msp;
    uint32_t psp;
    uint32_t exceptionReturn;
    uint32_t frameValid;
    uint32_t stackedPC;
    uint32_t stackedLR;
    uint32_t stackedXPSR;
} NWatchRtosFault;

extern volatile NWatchRtosDiagnostics g_nwatchRtos;
extern volatile NWatchRtosFault g_nwatchFault;

__attribute__((noreturn)) void NWatch_RtosStart(void (*guiTask)(void *));
void NWatch_RtosGuiEntered(void);
void NWatch_RtosGuiReady(void);
void NWatch_RtosPoll(void);
void NWatch_RtosAssert(const char *file, uint32_t line);
__attribute__((noreturn)) void NWatch_RtosFatal(uint32_t reason, const char *file, uint32_t line);

#endif
