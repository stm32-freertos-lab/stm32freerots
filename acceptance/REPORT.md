# STM32 FreeRTOS 最终移植验收报告

日期：2026-11-05（Asia/Shanghai）。结论：**当前单 GUI 应用版本的基本 FreeRTOS 移植验收通过。** 已取得 ST-Link/OpenOCD 在线调试证据，并结合用户的 OLED、菜单、动画/立方体、红外操作反馈。此结论不覆盖多个应用任务之间的抢占、时间片和同步行为。

用户自行完成烧录后，本轮复用了已运行的 OpenOCD，未重新烧录、擦除或修改目标固件。做过一次受控复位以实际验证 SVC 首任务启动。所有临时断点已触发并移除，最后一次有效采样结束时确认 CPU 为 running。报告收尾时 USB 枚举已不再发现 ST-Link，OpenOCD 返回 unknown，无法再次读取当前状态；此前有效验收证据保留，未再操作目标。原静态报告保存在 STATIC_REPORT.md。

| 检查项目 | 检查结果 | 验证方式 |
| --- | --- | --- |
| FreeRTOS 调度器 | 通过 | 实测 taskCreateResult=1、schedulerState=2、phase=3；SVC 首次启动后进入 GUI。 |
| FreeRTOS Tick | 通过 | 连续运行两个约 3 秒区间，内核 Tick 分别增加 3002、3001；Tick Hook 同步增加。 |
| GUI_Task 运行 | 通过 | 实际命中 GUI 首入口及循环；采样循环计数 4→803→1578。 |
| 任务切换 | 通过 | GUI 在延时列表时实际进入 Idle，GUI 随后恢复；PendSV 入口命中。仅限 GUI/Idle 基本切换范围。 |
| SysTick/PendSV/SVC | 通过 | 向量与 port.o 一致；实测 SVC 的 IPSR=11、PendSV 的 IPSR=14；SysTick LOAD=71999、CTRL 低三位=7，内核 Tick 持续增加。 |
| TIM3/TIM4 | 通过 | 读取 PSC/ARR 和连续计数：TIM3 每区间约增加 3,001,717/3,001,582µs；TIM4 每区间增加 3002/3002ms。 |
| Heap 使用 | 通过 | heap_4 总量 6144B，实测剩余和历史最小剩余均为 2384B，无 malloc 失败记录。 |
| 任务栈安全 | 通过 | 观测最低 GUI 水位 686 字、Idle 110 字；栈检测模式 2，未记录溢出。结论限本次实际覆盖路径。 |
| OLED/菜单/动画 | 通过 | 用户确认 OLED、菜单、动画/立方体均正常，连续操作无卡死；本轮未做自动视觉测试。 |
| 红外遥控 | 通过 | 用户确认红外操作正常；EXTI/时间戳链及优先级已核查。未做自动波形注入或长时间边界测试。 |

## 内存与运行数据

| 项目 | 实测/构建结果 |
| --- | --- |
| Flash 实际装载占用 | 35,676 / 65,536 B；余 29,860 B |
| RAM 静态 RW+ZI | 11,256 / 20,480 B；余 9,224 B |
| FreeRTOS Heap 总量 | 6144 B，已包含在上述静态 RAM 中 |
| xPortGetFreeHeapSize | 2384 B |
| xPortGetMinimumEverFreeHeapSize | 2384 B |
| GUI 配置栈 | 768 个 32 位字 = 3072 B，从 RTOS Heap 分配 |
| GUI 最低观测剩余水位 | 686 个 32 位字 = 2744 B；受控复位后的短时采样为 688 字 |
| Idle 配置栈/最低观测水位 | 128 字 / 110 字（440 B 剩余） |
| 应用/系统任务 | GUI 优先级 2；Idle 优先级 0；实测总任务数 2 |
| 故障 | g_nwatchFault.reason=0；CFSR=0、HFSR=0；观察期间未发现 HardFault、assert、malloc 失败或栈溢出 |

栈水位表示运行以来最少未使用栈空间，不是当前 PSP 到栈底的距离；不能由此保证未覆盖路径或无限期运行安全。主异常栈 1024B、C 库堆 512B 已计入静态 RAM；任务栈、TCB 从 6KB RTOS Heap 内分配，不可重复加到总 RAM 占用。

## 在线证据

1. 复用本机 OpenOCD 0.12/ST-Link V2，识别 Cortex-M3 和 64KiB Flash。CubeProgrammer 起初显示 DEV_CONNECT_ERR，原因是探针已被现有 OpenOCD 占用；未终止该用户进程。
2. GDB 比较 ER_IROM1 全部匹配；暂停状态下又读回完整 35,676B Flash（含 AC5 压缩的 RW 初始化数据），与本次 AXF 装载段逐字节一致。读回数据 SHA256：380737346fb606ee08ae313be7ed43a5fb002f43440fc4062093d283981639d1。
3. 在 vApplicationIdleHook 临时断点处，当前 TCB 为 IDLE（0x20001B70），GUI TCB 为 0x20001920。GUI 的 xStateListItem.pxContainer 等于 pxDelayedTaskList，距离唤醒为 1 Tick。随后在 NWatch_RtosPoll 命中，当前任务名为 NWatchGUI，CONTROL=2、BASEPRI=0、PRIMASK=0，证明任务上下文正确恢复。
4. 实际命中 PendSV_Handler，xPSR=0x6100000E，异常号为 14。
5. 受控复位后实际命中 SVC_Handler，xPSR=0x2100000B，异常号为 11；任务创建结果为 1，内核 xSchedulerRunning 为 1。随后命中 GUI 首入口，运行在线程/PSP 上下文，所有既有入口断言通过，UI 初始化到 phase=3。
6. 后续只在 CPU 暂停时读取运行变量，每次读取后恢复运行，连续运行约 3 秒再取下一快照。两次间隔的主机计时均约 3.031 秒，其中包含约 30ms 调试暂停/通信开销。

| 计数 | 快照 1 | 快照 2 | 快照 3 |
| --- | ---: | ---: | ---: |
| xTickCount | 178 | 3180 | 6181 |
| Tick Hook | 178 | 3180 | 6181 |
| GUI 循环 | 4 | 803 | 1578 |
| Idle Hook | 8 | 1572 | 3094 |
| HAL 原始 uwTick | 178 | 3180 | 6182 |
| Heap 当前/历史最小（B） | 2384 / 2384 | 2384 / 2384 | 2384 / 2384 |
| schedulerState | 2 | 2 | 2 |
| 故障 reason | 0 | 0 | 0 |

诊断中的 kernelTickCount、halMilliseconds、栈水位和 Heap 为约每秒采样值，可能落后于直接读取的内核 xTickCount/HAL uwTick，不能要求二者每一时刻相等。Idle Hook 计数不是上下文切换次数，切换证据来自上面的断点与延时列表检查。

## 时基、优先级和 GUI 边界

- SysTick：内核专用，72MHz/1000Hz，LOAD=71999；SVC、PendSV、SysTick 均由 RVDS/ARM_CM3 移植层提供，没有重复强入口。
- TIM3：PSC=71、ARR=65535，1MHz 微秒计时；TIM4：PSC=7199、ARR=9，HAL 1ms 时基。两者在调试暂停时冻结，故用连续运行区间的增量验收。
- AIRCR 优先级分组读值对应 NVIC_PRIORITYGROUP_4；TIM3/TIM4 优先级字节均为 0x40，EXTI15_10 为 0x50，PendSV/SysTick 为 0xF0。BASEPRI 阈值 0x50；高优先级计时 ISR 不调用 RTOS API。
- GUI 每轮调用 vTaskDelay(1 Tick)，已实测有效阻塞并让 Idle 执行。没有异常退出或观察窗口内的死循环证据。
- HAL_Delay、mdelay、udelay 已被当前镜像链接器移除。OLED 仍使用原阻塞式 I2C；正常使用已由用户确认，但 I2C 外设故障时的最长等待、最坏帧时间和恢复行为未专项测试。
- 本机运行态大块 SWD Flash 读回出现不一致，故这些数据未用于通过判定；暂停读回完全匹配。最终运行数据使用暂停快照，不能把调试暂停期间的时间差当作固件 Tick 异常。

## 修改范围与交付

固件仍为上一轮 AC5 Clean + Rebuild 产物（48 个输入文件、0 错误、9 个既有未使用函数警告）。本次上板验证没有再修改任何固件源码、配置、UI、菜单、动画、立方体、红外或 master 备份，也没有增加应用任务。

- final-verification.json：当前最终结论与关键数值；verification.json 保留首次静态检查结果，不能将其中历史 hardware_tested=false 误认为本次结论。
- runtime-check.log：Flash ER 比较、运行寄存器、GUI/Idle 切换及 PendSV 实测。
- startup-check.log：受控复位后的 SVC 首任务启动和 GUI 进入实测。
- runtime-observations.json / .log：三次暂停快照及连续运行增量；read_runtime.py 为对应主机侧读取脚本，其自身不复位目标。
- flash-read-comparison.json、board-flash-readback.bin：暂停读回完整 Flash 一致性证据；早期运行态读取差异仅用于记录调试读回限制。
- STATIC_REPORT.md：首次无 ST-Link 时的静态验收及完整手动步骤；源代码差异、构建记录和验收前备份仍保留。

**验收边界：可以认定现有单 GUI 应用版本的 FreeRTOS 基本移植成功。GUI 与 Idle 的切换、延时、唤醒已验证；多个应用任务间的优先级抢占、同优先级时间片、同步及竞争没有被充分验证。** 本轮到此停止，不进入多任务扩展。
