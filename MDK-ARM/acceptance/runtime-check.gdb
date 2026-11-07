# 仅连接和读取；临时硬件断点用于观察调度，结束时恢复运行。
set pagination off
set confirm off
set remotetimeout 8
target extended-remote localhost:3333
monitor halt
echo \n===== FLASH COMPARISON =====\n
compare-sections ER_IROM1
echo \n===== CURRENT STATE =====\n
source acceptance/observe.gdb
nwatch_snapshot
info registers control basepri primask
echo \n===== TIMERS AND PRIORITIES =====\n
x/3wx 0xe000e010
x/wx 0xe000ed08
x/wx 0xe000ed0c
x/wx 0xe000ed20
x/2bx 0xe000e41d
x/bx 0xe000e428
x/wx 0xe000ed28
x/wx 0xe000ed2c
x/wx 0x40000400
x/3wx 0x40000424
x/wx 0x40000800
x/3wx 0x40000824
x/hx 0x1ffff7e0
echo \n===== IDLE WHILE GUI DELAYED =====\n
tbreak vApplicationIdleHook
continue
p/x pxCurrentTCB
p ((TCB_t *)pxCurrentTCB)->pcTaskName
p/x 'rtos_app.c'::guiHandle
p ((TCB_t *)'rtos_app.c'::guiHandle)->xStateListItem.pxContainer == pxDelayedTaskList
p ((TCB_t *)'rtos_app.c'::guiHandle)->xStateListItem.xItemValue - xTickCount
echo \n===== GUI RESUMED =====\n
tbreak NWatch_RtosPoll
continue
p/x pxCurrentTCB
p ((TCB_t *)pxCurrentTCB)->pcTaskName
info registers control basepri primask
echo \n===== PENDSV ENTRY =====\n
tbreak PendSV_Handler
continue
p/x $xpsr
p/x $pc
echo \n===== RESUME AND DETACH =====\n
detach
quit
