# 受控复位验证首次 SVC 启动；仅使用临时硬件断点，不烧录。
set pagination off
set confirm off
set remotetimeout 8
target extended-remote localhost:3333
monitor halt
tbreak SVC_Handler
monitor reset halt
continue
echo \n===== SVC STARTUP =====\n
p/x $xpsr
p/x $pc
p g_nwatchRtos.taskCreateResult
p xSchedulerRunning
tbreak NWatch_RtosGuiEntered
continue
echo \n===== GUI FIRST ENTRY =====\n
p/x $xpsr
info registers control basepri primask
p ((TCB_t *)pxCurrentTCB)->pcTaskName
tbreak NWatch_RtosPoll
continue
echo \n===== GUI READY =====\n
source acceptance/observe.gdb
nwatch_snapshot
echo \n===== DETACH =====\n
detach
quit
