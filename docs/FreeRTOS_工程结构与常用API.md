# FreeRTOS 工程结构与常用 API（NWatch 长期学习文档）

核查日期：2026-11-07（Asia/Shanghai）。版本依据：本地 `ThirdParty/FreeRTOS-Kernel/include/task.h` 的 `tskKERNEL_VERSION_NUMBER="V11.3.1"`，并交叉核对 `phase3_freertos/source-origin.json`。

本文根据真实目录、源码、Keil/EIDE 工程配置、已有编译依赖和链接 map 编写。本轮只读取工程并生成本文，没有构建、改动源程序或配置，也没有连接、复位或烧录开发板。运行数据引用既有验收记录，不代表本轮新测。

## 1. 先记住这几个位置

- 官方内核：`MDK-ARM/ThirdParty/FreeRTOS-Kernel/`。
- 当前移植：`portable/RVDS/ARM_CM3/port.c`、`portmacro.h`。
- 当前内存管理：`portable/MemMang/heap_4.c`。
- 内核配置：`MDK-ARM/RTOS/FreeRTOSConfig.h`。
- 日常新增任务、通信和诊断：`MDK-ARM/RTOS/`。
- **现有 GUI_Task 定义在 `Core/Src/main.c`，不是 rtos_app.c，也没有单独的 gui_task.c。**
- `RTOS/rtos_app.c` 通过 `NWatch_RtosStart(GUI_Task)` 接收任务入口，创建 GUI，再启动调度器。
- 当前只有一个用户任务 GUI，以及内核自动创建的 Idle。还没有 Input_Task，也没有用户队列对象。

路径约定：主结构树从工程根 `NWatch-DShanMCU-F103-RTOS/` 开始；其余没有特别说明的路径以 **MDK-ARM/** 为基准。`../Core/` 和 `../NWatch/` 是实际相对位置。本文保存位置为 `MDK-ARM/docs/FreeRTOS_工程结构与常用API.md`。

## 2. 如何判断“当前可用”

必须分清以下四件事：

| 层次 | 核查依据 | 本项目的例子 |
| --- | --- | --- |
| 文件存在 | 实际递归扫描文件系统 | timers.c、event_groups.c 都在源码包里 |
| 源文件参与编译 | uvprojx 的文件/排除项，EIDE sourceList 和已有 .o/.d | queue.c 参与编译；event_groups.c 没加入构建 |
| 功能通过条件编译 | FreeRTOSConfig.h 与 FreeRTOS.h 默认值及实现中的 #if | vTaskDelete 被关闭；任务通知被关闭 |
| 代码留在最终镜像且正在使用 | map 的保留/Removing 节、应用调用和既有运行记录 | queue.o 已编译，但当前队列函数均未使用，被链接器移除 |

头文件不会像 .c 一样独立编译。下文“头文件已用”表示已有 .d 依赖记录；“头文件可包含”表示它位于有效 include 路径，但尚无实际包含证据。

API 表中“可用”表示**按当前配置可以编写调用并正常重新构建**，不表示已经有对象、已经调用，或能够在当前烧录镜像中由调试器直接调用。新增调用后重新链接，原先被裁剪的函数才会保留。

核查的现行构建依据：

1. `NWatch-DShanMCU-F103.uvprojx`：唯一目标 NWatch-DShanMCU-F103，配置记录AC5 V5.06 update 7，STM32F103C8。现存map头部记录实际生成它的armlink为ARM Compiler 5.06 update 5（build 528）；工程工具链选择记录与历史产物的实际版本要区分。
2. `.eide/eide.yml`：现行文件列表、excludeList、include 路径，与 Keil 的 FreeRTOS 选项一致。
3. `build/NWatch-DShanMCU-F103/builder.params`：现存构建实际 sourceList，包含 48 个源文件。
4. `build/NWatch-DShanMCU-F103/.obj/` 中的 .d/.o，以及同目录 map。
5. `acceptance/REPORT.md`：2026-11-05 既有验收与 RAM 数据。

`Nwatch-DShanMCU-F103/`、`phase1/`、`phase2_timebase/`、`phase3_freertos/kernel-bringup/` 和 `acceptance/before/` 存在历史产物/备份，不能把这些 map 或旧源码混作当前主构建。文件仍在磁盘上也不等于参与现行编译。

## 3. 真实工程结构树

下树列出学习和开发需要的真实路径；官方包其他架构文件的完整逐文件清单见附录 A，避免主树淹没在几百个未选中的移植文件中。

~~~text
NWatch-DShanMCU-F103-RTOS/
├─ Core/
│  ├─ Inc/
│  │  ├─ main.h                       【STM32驱动】主程序/板级声明
│  │  ├─ stm32f1xx_it.h               【STM32驱动】中断声明
│  │  ├─ stm32f1xx_hal_conf.h         【STM32驱动】HAL配置，Tick中断优先级4
│  │  └─ tim.h                       【STM32驱动】旧外设声明，非当前时基实现
│  └─ Src/
│     ├─ main.c                      【用户应用】【STM32驱动】main与现有GUI_Task
│     ├─ stm32f1xx_it.c               【STM32驱动】EXTI、TIM3/TIM4及故障入口
│     ├─ stm32f1xx_hal_msp.c          【STM32驱动】底层时钟/GPIO/优先级分组
│     ├─ system_stm32f1xx.c           【STM32驱动】系统时钟与SystemCoreClock
│     └─ tim.c                       【STM32驱动】存在，未加入当前构建
├─ Drivers/
│  ├─ DShanMCU-F103/
│  │  ├─ driver_timer.c / .h         【STM32驱动】TIM3微秒计时，IRQ优先级4
│  │  ├─ driver_systick.c / .h       【STM32驱动】名称虽含SysTick，实际用TIM4给HAL计时
│  │  ├─ driver_irq.c / .h           【STM32驱动】EXTI回调转交红外
│  │  ├─ driver_ir_receiver.c / .h   【STM32驱动】红外采样/解析/读取
│  │  ├─ driver_lcd.c / .h           【STM32驱动】显示适配接口
│  │  ├─ driver_oled.c / .h          【STM32驱动】OLED/I2C及1024B帧缓冲
│  │  └─ driver_timertmp.c           【STM32驱动】存在，未加入当前构建的旧文件
│  ├─ STM32F1xx_HAL_Driver/
│  │  ├─ Inc/                       【STM32驱动】HAL接口
│  │  └─ Src/                       【STM32驱动】HAL实现，当前只选中部分
│  └─ CMSIS/                        【STM32驱动】Cortex-M3及芯片寄存器定义
├─ NWatch/                           【NWatch GUI】原有单GUI状态机，不是一个文件一个RTOS任务
│  ├─ buttons.c / .h                【NWatch GUI】当前输入轮询/按键回调
│  ├─ display.c / .h                【NWatch GUI】刷新调度、CRT等显示行为
│  ├─ draw.c / .h                   【NWatch GUI】绘图函数
│  ├─ animation.c / .h              【NWatch GUI】动画状态
│  ├─ millis.c / .h                 【NWatch GUI】经SysTickGetMs读取HAL/TIM4毫秒
│  ├─ m_main.c / .h                 【NWatch GUI】主菜单
│  ├─ m_display.c / .h              【NWatch GUI】显示菜单
│  ├─ menu.c / .h                   【NWatch GUI】菜单机制
│  ├─ normal.c / .h                 【NWatch GUI】普通表盘
│  ├─ tunemaker.c / .h              【NWatch GUI】当前立方体等演示
│  ├─ watchface.c / .h              【NWatch GUI】表盘/演示及绘图数据
│  ├─ resources.c / .h              【NWatch GUI】只读图像/字库等资源主要在Flash
│  ├─ appconfig.c / .h              【NWatch GUI】应用配置状态
│  ├─ global.c / .h                 【NWatch GUI】全局更新接口
│  ├─ pwrmgr.c / .h                 【NWatch GUI】活动/电源管理
│  ├─ time.c / .h                   【NWatch GUI】时间更新
│  └─ ui.c / .h                     【NWatch GUI】UI入口与公共接口
└─ MDK-ARM/
   ├─ RTOS/
   │  ├─ FreeRTOSConfig.h           【内核配置】本项目真正使用的配置
   │  ├─ rtos_app.c                 【用户应用】创建GUI、启动调度、钩子、故障与诊断
   │  └─ rtos_app.h                 【用户应用】接口、GUI栈/优先级、诊断结构
   ├─ ThirdParty/FreeRTOS-Kernel/
   │  ├─ tasks.c                    【官方内核】已编译，任务与调度
   │  ├─ queue.c                    【官方内核】已编译，队列/信号量/互斥量公共实现
   │  ├─ list.c                     【官方内核】已编译，内核链表
   │  ├─ event_groups.c             【官方内核】存在，未加入构建
   │  ├─ timers.c                   【官方内核】存在，未加入构建且configUSE_TIMERS=0
   │  ├─ stream_buffer.c            【官方内核】存在，未加入构建
   │  ├─ croutine.c                 【官方内核】存在，协程未启用
   │  ├─ include/
   │  │  ├─ FreeRTOS.h              【官方内核】公共配置入口/默认值/类型
   │  │  ├─ task.h                  【官方内核】任务和任务通知API
   │  │  ├─ queue.h                 【官方内核】队列API
   │  │  ├─ semphr.h                【官方内核】信号量/互斥量宏，底层在queue.c
   │  │  ├─ list.h                  【官方内核】内部链表
   │  │  ├─ timers.h                【官方内核】软件定时器API声明/宏
   │  │  ├─ event_groups.h          【官方内核】事件组API
   │  │  ├─ stream_buffer.h         【官方内核】流缓冲区API
   │  │  ├─ message_buffer.h        【官方内核】消息缓冲区宏，依托stream_buffer.c
   │  │  ├─ portable.h              【移植层】移植接口和堆API声明
   │  │  └─ 其他真实头文件           完整路径/作用见第4节和附录A
   │  └─ portable/
   │     ├─ RVDS/ARM_CM3/
   │     │  ├─ port.c               【移植层】当前使用，SVC/PendSV/SysTick
   │     │  └─ portmacro.h          【移植层】当前使用，栈类型/临界区/ISR切换宏
   │     ├─ MemMang/
   │     │  ├─ heap_4.c             【内存管理】当前唯一堆实现，合并相邻空闲块
   │     │  ├─ heap_1.c             【内存管理】存在，未选中
   │     │  ├─ heap_2.c             【内存管理】存在，未选中
   │     │  ├─ heap_3.c             【内存管理】存在，未选中
   │     │  └─ heap_5.c             【内存管理】存在，未选中
   │     └─ 其他实际架构目录         【移植层】均未用于本目标，附录A逐文件列出
   ├─ startup_stm32f103xb.s          【STM32驱动】当前启动文件，向量/MSP/C库堆
   ├─ NWatch-DShanMCU-F103.uvprojx    Keil编译配置，只读核查
   ├─ .eide/eide.yml                 EIDE编译配置，只读核查
   ├─ build/NWatch-DShanMCU-F103/     当前构建证据：builder.params、map、.obj
   ├─ scripts/                      历史移植/核查脚本，不是目标固件源码
   ├─ phase3_freertos/               官方包来源记录、压缩包与早期构建
   ├─ acceptance/                   已有验收记录与旧备份
   └─ docs/
      └─ FreeRTOS_工程结构与常用API.md 本文
~~~

## 4. 文件分类、作用与真实编译状态

### A. 官方内核和公共头文件

以下路径均在 `ThirdParty/FreeRTOS-Kernel/` 下。

| 实际文件 | 作用 | 当前状态 |
| --- | --- | --- |
| tasks.c + include/task.h | 任务创建/调度/延时/通知和查询 | .c已编译、镜像保留必要函数；.h已用；部分API被配置关闭 |
| queue.c + include/queue.h | 队列、信号量、互斥量的公共基础 | .c已编译、.h已用；队列函数当前未调用并被链接裁剪；不能说“没有队列实现” |
| list.c + include/list.h | 就绪/阻塞等内核链表 | .c已编译且使用；.h已用 |
| timers.c + include/timers.h | 软件定时器及延后执行服务 | .c存在但未加入构建；.h有内核依赖记录；TIMERS=0，功能不可用 |
| event_groups.c + include/event_groups.h | 多条件事件位同步 | .c未加入构建；.h存在可包含但当前未用；配置默认允许也不等于已链接 |
| stream_buffer.c + include/stream_buffer.h | 字节流单写者/单读者通信 | .c未加入构建；通知等依赖未满足 |
| include/message_buffer.h | 变长消息边界，在流缓冲基础上封装 | 存在未用；没有单独的message_buffer.c |
| croutine.c + include/croutine.h | 旧式协程机制 | 存在未编译；configUSE_CO_ROUTINES=0 |
| include/semphr.h | 二值/计数信号量、普通/递归互斥量API宏 | 存在、当前应用未包含；二值信号量现有配置可用，其他种类分别受配置控制 |
| include/FreeRTOS.h | 包含应用配置、移植接口和默认选项 | 已被实际包含，不独立编译 |
| include/projdefs.h | pdPASS、pdTRUE、任务入口类型、毫秒换Tick等 | 实际头依赖 |
| include/deprecated_definitions.h | 旧版本类型/名称兼容声明 | 实际头依赖；BACKWARD_COMPATIBILITY=0仍可能包含此文件 |
| include/stack_macros.h | 栈溢出检查实现宏 | 实际头依赖，当前检查模式2 |
| include/StackMacros.h | 旧拼写文件，提示改用stack_macros.h | 存在；不应作为新代码入口 |
| include/atomic.h | 原子操作辅助 | 存在未作为当前头依赖 |
| include/mpu_wrappers.h | MPU接口包装选择；普通端口下许多宏为空 | 实际头依赖，不能由此推断已启用MPU |
| include/mpu_prototypes.h | MPU受保护API原型 | 存在，当前未用MPU端口 |
| include/mpu_syscall_numbers.h | MPU系统调用编号 | 存在，当前未用 |
| include/newlib-freertos.h | Newlib每任务C库状态支持 | 存在未用，当前是AC5且NEWLIB_REENTRANT=0 |
| include/picolibc-freertos.h | Picolibc每任务C库状态支持 | 存在未用 |
| include/stdint.readme | 整数类型说明 | 说明文件，不编译 |
| include/CMakeLists.txt | CMake头文件组织 | 存在，当前Keil/EIDE不使用CMake构建 |

### B. 移植层

| 实际路径 | 作用 | 当前状态 |
| --- | --- | --- |
| ThirdParty/FreeRTOS-Kernel/portable/RVDS/ARM_CM3/port.c | 为AC5/Cortex-M3准备初始任务栈、启动首任务、保存/恢复上下文、处理内核Tick、临界区及中断优先级校验 | 当前编译并使用 |
| ThirdParty/FreeRTOS-Kernel/portable/RVDS/ARM_CM3/portmacro.h | StackType_t、BaseType_t、TickType_t、BASEPRI和portYIELD_FROM_ISR等 | 当前实际包含 |
| ThirdParty/FreeRTOS-Kernel/include/portable.h | 上层统一的移植接口、内存接口声明 | 当前实际包含 |
| portable/下其余架构文件 | 别的CPU、编译器、MPU/安全上下文、模拟器所需实现 | 扫描确认存在；本项目未选择，逐文件见附录A |

不要因为工具链中看到“Keil”就改选 `portable/Keil/`；当前实际选择是 **RVDS/ARM_CM3 + AC5**，不能混搭不同编译器的汇编入口。

### C. 内存管理

| 实际路径 | 作用 | 当前状态 |
| --- | --- | --- |
| ThirdParty/FreeRTOS-Kernel/portable/MemMang/heap_4.c | 在ucHeap数组中分配/释放，合并相邻空闲块 | 已编译，唯一现行FreeRTOS堆 |
| 同目录 heap_1.c | 只分配不释放的简化策略 | 存在，未编译 |
| 同目录 heap_2.c | 可释放但不合并相邻空闲块 | 存在，未编译 |
| 同目录 heap_3.c | 包装C库malloc/free | 存在，未编译 |
| 同目录 heap_5.c | 支持多个不连续内存区域 | 存在，未编译 |
| 同目录 ReadMe.url | 官方内存管理文档快捷链接 | 不编译 |

一次只选择一个堆实现。本项目的 `pvPortMalloc` 不是C库 `malloc`，不能交叉释放。

### D. 配置文件

| 实际路径 | 作用 | 当前状态 |
| --- | --- | --- |
| RTOS/FreeRTOSConfig.h | 本目标的内核开关、6KiB堆、时钟、钩子、中断映射 | 当前真正包含的配置 |
| ThirdParty/FreeRTOS-Kernel/examples/template_configuration/FreeRTOSConfig.h | 官方示例配置模板 | 存在，未使用，修改它不会改变本目标 |
| ThirdParty/FreeRTOS-Kernel/examples/coverity/FreeRTOSConfig.h | 静态分析示例配置 | 存在，未使用 |

真正配置已由 `rtos_app.d` 等依赖确认，不能根据同名文件猜测。

### E. 我们自己的应用

| 实际路径 | 作用 | 当前状态 |
| --- | --- | --- |
| RTOS/rtos_app.c | NWatch_RtosStart创建GUI和启动调度；每秒采样堆/栈/任务状态；Tick/Idle/malloc失败/栈溢出钩子；断言和HardFault诊断 | 编译使用 |
| RTOS/rtos_app.h | 任务配置常量、诊断/故障结构、对外函数声明 | 实际包含 |
| ../Core/Src/main.c 内的static GUI_Task | 初始化NWatch，循环处理输入/时间/界面，调用NWatch_RtosPoll，每轮vTaskDelay(1 Tick) | 编译使用；不是独立文件 |
| ../NWatch/buttons.c | 目前由GUI循环驱动的输入逻辑 | 编译使用；还不是独立Input_Task |
| RTOS/input_task.c、RTOS/input_task.h | 将来的独立输入任务建议位置 | **当前不存在**，本轮不创建 |
| RTOS/app_queue.c、RTOS/app_queue.h | 将来的消息类型、队列句柄及初始化建议位置 | **当前不存在**，本轮不创建 |

`rtos_app.c` 中还有 `NWatch_RtosGuiEntered`（确认PSP/调度器）、`NWatch_RtosGuiReady`（初始化完成）、`NWatch_RtosPoll`（周期诊断）。这些是本工程封装，不是官方API。

### F. 与FreeRTOS相接的STM32底层与GUI文件

| 实际路径 | 与RTOS的关系 | 当前状态 |
| --- | --- | --- |
| ../Core/Src/main.c | 初始化72MHz时钟、GPIO/I2C/TIM3，设置EXTI优先级，传GUI入口给RTOS | 编译使用 |
| ../Core/Src/stm32f1xx_it.c / ../Core/Inc/stm32f1xx_it.h | TIM3/TIM4、红外EXTI和异常声明；没有另写C版SVC/PendSV/SysTick强入口 | .c编译、.h包含 |
| startup_stm32f103xb.s | 向量表、Reset_Handler、1024B主栈、512B C库堆 | 当前启动文件，参与汇编/链接 |
| ../Core/Src/stm32f1xx_hal_msp.c | HAL底层初始化、NVIC_PRIORITYGROUP_4 | 编译使用 |
| ../Core/Src/system_stm32f1xx.c | SystemInit与SystemCoreClock | 编译使用 |
| ../Core/Inc/stm32f1xx_hal_conf.h | TICK_INT_PRIORITY=4，影响HAL TIM4中断 | 实际包含 |
| ../Drivers/DShanMCU-F103/driver_timer.c / .h | TIM3给system_get_us微秒计数，优先级4，不调用RTOS | .c编译、.h包含 |
| ../Drivers/DShanMCU-F103/driver_systick.c / .h | 重写HAL_InitTick，使用TIM4给HAL毫秒计时；SysTickGetMs只是兼容名称 | .c编译、.h包含 |
| ../Drivers/DShanMCU-F103/driver_irq.c / .h | HAL_GPIO_EXTI_Callback转给红外回调 | .c编译、.h存在供板级接口使用 |
| ../Drivers/DShanMCU-F103/driver_ir_receiver.c / .h | EXTI边沿读取TIM3时间戳、解析红外并提供IRReceiver_Read | .c编译、.h包含，当前没有FreeRTOS通信调用 |
| ../Drivers/DShanMCU-F103/driver_lcd.c / .h | GUI到OLED接口 | 编译使用，保持单GUI访问 |
| ../Drivers/DShanMCU-F103/driver_oled.c / .h | I2C OLED与帧缓冲，当前阻塞式发送 | 编译使用，保持单GUI访问 |
| ../Core/Src/tim.c / ../Core/Inc/tim.h | 原TIM1/TIM2外设初始化/声明 | .c存在但未加入构建；tim.h有旧代码包含，不等于tim.c编译 |
| ../Drivers/DShanMCU-F103/driver_timertmp.c | 旧定时器文件 | 存在、未加入构建；不是TIM3/TIM4现行实现 |
| ../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c | HAL初始化/Tick接口，被板级TIM4实现覆盖相应弱函数 | 编译使用 |
| 同目录 stm32f1xx_hal_cortex.c | NVIC分组与优先级操作 | 编译使用 |
| 同目录 stm32f1xx_hal_tim.c、stm32f1xx_hal_tim_ex.c | TIM底层HAL | 两者源文件参与编译；tim_ex当前未用函数可能全部裁剪 |
| 同目录 stm32f1xx_hal_timebase_tim_template.c | ST提供的通用HAL时基模板 | 存在、未加入构建，当前使用driver_systick.c |
| ../Drivers/CMSIS/RTOS/Template/cmsis_os.h | 通用CMSIS-RTOS接口模板 | 存在但不是FreeRTOS适配实现 |
| ../Drivers/CMSIS/RTOS2/Include/cmsis_os2.h、RTOS2/Template/cmsis_os.h、cmsis_os1.c | CMSIS-RTOS2接口/兼容模板 | 存在未用于当前原生FreeRTOS应用 |
| ../Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/{arm,gcc,iar}/startup_stm32f103xb.s | 厂商分发的其他启动模板副本 | 存在但均不是当前编译选中的启动文件 |

同目录HAL `rcc.c/rcc_ex.c/gpio.c/gpio_ex.c/i2c.c/dma.c/exti.c/pwr.c`（文件均带 `stm32f1xx_hal_` 前缀）也在当前sourceList中，为时钟/显示/中断等提供基础支持；是否保留函数由链接器决定。`stm32f1xx_hal_spi.c`、`stm32f1xx_hal_uart.c` 已明确排除。

当前参与编译的NWatch文件就是第3节主树中的17个.c。以下同目录文件存在但被工程排除：`alarm.c, alarms.c, game1.c, game2.c, game3.c, games.c, settings.c, stopwatch.c, timedate.c, torch.c, tune.c, tunes.c, sound.c, sleep.c, diag.c, reset.c`。不要把它们当成已经运行的任务。

当前板级驱动明确排除：`driver_color_led.c, driver_uart.c, driver_passive_buzzer.c, gui_log_console.c, driver_dht11.c, driver_spiflash_w25q64.c`。它们是外设/旧应用功能，不能仅因在工程分组中显示就算启用。

**核实不存在的常见文件：** 当前没有独立的 `semphr.c`（实现在queue.c）、`message_buffer.c`（实现在stream_buffer.c）、`RTOS/gui_task.c`、`RTOS/input_task.c`、`Core/Src/freertos.c`、本项目专用的 `stm32f1xx_hal_timebase_tim.c`，也没有已接入FreeRTOS的CMSIS-RTOS适配层。官方示例和CMSIS模板不是本应用。

### 构建、来源与历史资料的性质

| 实际路径 | 用途 | 编译状态 |
| --- | --- | --- |
| scripts/fetch_freertos.py | 获取官方内核包并记录来源 | 主机脚本，不进固件；本轮不运行 |
| scripts/configure_freertos.cjs | 历史工程配置集成脚本 | 主机脚本，不进固件；本轮不运行 |
| scripts/integrate_freertos.py | 历史main/中断接入脚本 | 主机脚本，不进固件；本轮不运行 |
| scripts/verify_freertos.py | 历史静态/构建证据核查脚本 | 主机脚本，不进固件；本轮不运行 |
| phase3_freertos/source-origin.json | 官方V11.3.1下载来源、包SHA256、选择的端口记录 | 数据文件，不编译 |
| phase3_freertos/FreeRTOS-KernelV11.3.1.zip | 官方发布包留档 | 压缩包，不编译 |
| phase3_freertos/kernel-bringup/ | 早期仅内核启动镜像 | 历史产物，不代表当前完整GUI |
| acceptance/REPORT.md、STATIC_REPORT.md及JSON记录 | 既有静态与运行验收证据 | 文档，不编译 |
| acceptance/before/rtos_app.c、rtos_app.h及map | 验收前的备份 | 不参与现行构建 |
| phase1/、phase2_timebase/下的before/与baseline | 之前裁剪和时基阶段留档 | 不参与现行构建 |

## 5. 哪些文件可以修改

本节是以后开发时的建议；**本轮全部保持不变**。

| 修改级别 | 文件/模块 | 原因与边界 |
| --- | --- | --- |
| 建议不要修改 | 官方tasks.c/queue.c/list.c等、include/公共接口、RVDS/ARM_CM3端口 | 调度/中断现场/链表有严格不变量；改动难验证，也妨碍跟随官方版本。应用问题先排查调用、配置和优先级 |
| 建议不要修改 | heap_4.c及其他官方堆实现 | 分配器正确性敏感。需要容量调整时评估应用配置，不要随意改算法 |
| 经常需要修改 | RTOS/rtos_app.c、rtos_app.h，以后新增input_task.c/.h、app_queue.c/.h等 | 这是创建任务、定义接口、安排应用间通信和增加诊断的位置。故障捕获汇编虽在rtos_app.c中，也不应随手修改 |
| 按需要修改 | RTOS/FreeRTOSConfig.h、中断优先级配置、任务通信模块 | 新功能可能需要开关/源文件/堆容量共同匹配；变化会影响RAM与时序，必须成组核对 |
| 原则上尽量保持 | NWatch绘图/动画/菜单状态机、HAL及板级时基驱动 | 当前显示与计时已通过验收，多任务化先在RTOS应用边界接入，继续让GUI独占绘制状态和OLED |
| 接入时少量修改 | Core/Src/main.c内GUI_Task周边 | 现有任务入口确实在这里；未来队列接收需要明确接入点，不必为了新增输入任务大搬UI或重移植 |

“允许修改”不等于应当立即改。Input_Task拆分要先确定输入数据的唯一消费者以及GUI的消费位置，不能两个任务同时调用当前 `buttons_update()`。

## 6. 当前启用和未启用的功能

所有值均从本地配置或FreeRTOS.h默认值核对。

| 功能/配置 | 当前值/构建状态 | 现在能做什么；以后启用条件 |
| --- | --- | --- |
| 单核抢占/同优先级时间片 | NUMBER_OF_CORES=1；USE_PREEMPTION=1；USE_TIME_SLICING=1 | 当前生效；还没有多个用户任务的实用验证 |
| 任务优先级 | MAX_PRIORITIES=4 | 可用0～3，数字越大任务优先级越高；GUI=2，Idle=0 |
| 内核Tick | 1000Hz，32位 | 1 Tick=1ms；32位计数约49.7天回绕，用无符号差值处理 |
| 动态分配 | SUPPORT_DYNAMIC_ALLOCATION=1；heap_4 | xTaskCreate、队列/二值信号量动态创建可用 |
| 静态创建 | SUPPORT_STATIC_ALLOCATION=0 | xTaskCreateStatic/xQueueCreateStatic等不可用；将来启用需静态存储与Idle等内存供应方案 |
| RTOS堆 | TOTAL_HEAP_SIZE=6144；APPLICATION_ALLOCATED_HEAP=0 | 由heap_4.c内部ucHeap提供，不是独立于静态RAM的额外6KiB |
| 普通任务延时 | INCLUDE_vTaskDelay=1 | 当前GUI已用 |
| 固定节拍延时 | INCLUDE_xTaskDelayUntil=0 | xTaskDelayUntil不可链接；将来设1，无需新增内核.c |
| 删除任务 | INCLUDE_vTaskDelete=0 | vTaskDelete未编译；将来设1 |
| 挂起/恢复任务 | INCLUDE_vTaskSuspend=0 | 当前未启用；portMAX_DELAY不是无限等待的特殊语义 |
| 调整/读取任务优先级 | INCLUDE_vTaskPrioritySet=0；INCLUDE_uxTaskPriorityGet=0 | 对应API未编译；将来分别设1 |
| 调度器/Idle句柄/栈水位查询 | INCLUDE_xTaskGetSchedulerState=1；INCLUDE_xTaskGetIdleTaskHandle=1；INCLUDE_uxTaskGetStackHighWaterMark=1 | 当前诊断已用 |
| 队列 | queue.c已编译，没有configUSE_QUEUES总开关 | 创建/发送/接收可用；目前没有用户队列实例，函数在旧镜像被裁剪 |
| 二值信号量 | 依托queue.c，无单独USE_BINARY_SEMAPHORES开关 | 当前可创建、take/give；初值为空 |
| 计数信号量 | USE_COUNTING_SEMAPHORES=0 | 将来设1；创建宏可能可见，但底层xQueueCreateCountingSemaphore未编译 |
| 普通互斥量 | USE_MUTEXES=0 | 将来设1；不需要添加semphr.c |
| 递归互斥量 | USE_RECURSIVE_MUTEXES=0 | 将来同时设USE_MUTEXES=1及USE_RECURSIVE_MUTEXES=1 |
| 任务通知 | USE_TASK_NOTIFICATIONS=0 | 所列通知API当前均不可用；将来设1；默认通知数组长度为1，未索引API操作索引0 |
| 事件组 | USE_EVENT_GROUPS未显式设置，FreeRTOS.h默认1；event_groups.c未入构建 | 任务上下文API当前仍不可用；将来将.c加入Keil/EIDE，两者均要一致；可显式设USE_EVENT_GROUPS=1 |
| ISR事件组置位 | TIMERS=0；INCLUDE_xTimerPendFunctionCall默认0 | 将来还需timers.c、TIMERS=1、INCLUDE_xTimerPendFunctionCall=1及定时器配置 |
| 软件定时器 | USE_TIMERS=0；timers.c未入构建 | 将来设1、加入timers.c、设置configTIMER_TASK_PRIORITY/configTIMER_QUEUE_LENGTH/configTIMER_TASK_STACK_DEPTH，并评估守护任务栈/TCB和命令队列RAM |
| 流/消息缓冲区 | USE_STREAM_BUFFERS默认1，但stream_buffer.c未入构建 | 当前不可用；还需USE_TASK_NOTIFICATIONS=1、INCLUDE_xTaskGetCurrentTaskHandle=1，加入stream_buffer.c |
| 协程 | USE_CO_ROUTINES=0，croutine.c未编译 | 当前未用；新任务开发无需启用旧协程 |
| Tickless Idle | USE_TICKLESS_IDLE=0 | 当前Idle Hook仅WFI，不等于停止Tick/STOP模式 |
| 诊断保护 | CHECK_FOR_STACK_OVERFLOW=2；MALLOC_FAILED_HOOK/IDLE_HOOK/TICK_HOOK=1；configASSERT已接入 | 当前启用，失败记录在g_nwatchFault |
| 跟踪/格式化统计/运行时间统计 | USE_TRACE_FACILITY=0；USE_STATS_FORMATTING_FUNCTIONS=0；GENERATE_RUN_TIME_STATS=0 | 不可直接用vTaskList/vTaskGetRunTimeStats等；以后需对应开关及运行统计时基 |
| 队列注册表 | QUEUE_REGISTRY_SIZE=0 | 当前未为调试器登记队列 |
| 旧名称兼容 | ENABLE_BACKWARD_COMPATIBILITY=0 | 不应照搬旧教程类型；但vTaskDelayUntil宏仍实际存在，见第9节 |
| Newlib可重入 | USE_NEWLIB_REENTRANT=0 | 当前AC5项目；不能据此保证多任务printf/malloc等C库调用自动安全 |

重要区别：当前**事件组/流缓冲区不是被对应USE宏设成0**，而是未选中源文件（流缓冲还缺依赖）；通知、互斥量、定时器则有明确的关闭宏。

## 7. 当前任务和时基架构

~~~mermaid
flowchart TD
    Reset[Reset_Handler / SystemInit] --> Main[main: HAL / 72MHz / TIM3 / GPIO / I2C]
    Main --> Start[NWatch_RtosStart: 创建NWatchGUI / 启动调度器]
    Start --> GUI[GUI_Task 位于Core/Src/main.c<br/>优先级2 / 768字]
    Start --> Idle[内核Idle任务<br/>优先级0 / 128字]
    GUI --> Init[初始化NWatch与OLED]
    Init --> Loop[原NWatch循环<br/>buttons / time / global / display / pwrmgr]
    Loop --> Poll[NWatch_RtosPoll: 约每秒更新诊断]
    Poll --> Delay[vTaskDelay: 1 Tick]
    Delay --> Loop
    Delay -. 阻塞时允许运行 .-> Idle
    Idle --> WFI[vApplicationIdleHook / WFI]
    Sys[SysTick / port.c / 优先级15] --> Tick[内核1ms Tick与TickHook]
    Tick -. 唤醒到期任务 .-> GUI
    P[PendSV / port.c / 优先级15] -. 保存恢复任务上下文 .-> GUI
    T4[TIM4 / 优先级4] --> HAL[HAL_IncTick / HAL_GetTick]
    HAL --> Millis[NWatch millis与动画时间]
    T3[TIM3 / 优先级4] --> Micro[system_get_us微秒计时]
    EXTI[EXTI15_10 / 优先级5] --> IR[driver_irq回调 / 红外解析与缓存]
    Micro --> IR
    IR -. GUI中的IRReceiver_Read .-> Loop
~~~

- GUI任务使用PSP，异常处理使用MSP。任务栈不是startup里的主栈。
- SVC启动首任务；SVC/PendSV/SysTick符号通过FreeRTOSConfig.h映射到端口原始入口。不要再添加普通C包装入口。
- TIM3/TIM4中断为4，不允许调用会操作内核的FromISR通信API；它们继续负责原微秒/HAL时基。
- 现有红外回调中包含采样与解析，没有FreeRTOS队列发送。不要把将来的“ISR仅发事件”架构说成当前已经实现。
- `vTaskDelay(1)` 是让本轮GUI阻塞到后续Tick，不保证OLED固定1kHz刷新；刷新还受原display_update逻辑和I2C传输限制。


## 8. 常用API分类速查表

包含顺序通常是先 `#include "FreeRTOS.h"`，再包含 `task.h`、`queue.h`、`semphr.h` 等。堆函数声明在 `portable.h`，通常已由 `FreeRTOS.h` 引入。

表中“ISR可”仍要求：中断优先级合法、对象/目标任务已有效创建、调度启动时序正确，并按需要请求切换。普通API的等待时间为0，**也不会自动变成ISR安全API**。

### 8.1 任务管理

| API 函数 | 所属头文件 | 作用 | 使用场景 | 能否在中断中调用 | 当前工程是否可用 |
| --- | --- | --- | --- | --- | --- |
| xTaskCreate() | task.h | 动态创建任务 | 创建GUI、以后创建Input_Task | 否 | 是，已用于GUI |
| vTaskDelete() | task.h | 删除指定任务，NULL表示自身 | 结束一次性任务 | 否 | 否；INCLUDE_vTaskDelete需设1 |
| vTaskDelay() | task.h | 相对当前时刻阻塞若干Tick | GUI让出CPU、输入轮询间隔 | 否 | 是，GUI已用 |
| xTaskDelayUntil() | task.h | 按上一目标唤醒时刻维持周期 | 以后Input_Task按固定周期扫描 | 否 | 否；INCLUDE_xTaskDelayUntil需设1 |
| vTaskDelayUntil() | task.h | 调用xTaskDelayUntil并忽略返回值的旧形式宏 | 理解旧教程 | 否 | 宏存在，当前底层关闭；同样需INCLUDE_xTaskDelayUntil=1 |
| vTaskPrioritySet() | task.h | 设置任务优先级 | 明确任务时序后调整调度关系 | 否 | 否；INCLUDE_vTaskPrioritySet需设1 |
| uxTaskPriorityGet() | task.h | 获取任务当前优先级 | 调试任务优先级 | 否；应选uxTaskPriorityGetFromISR | 否；INCLUDE_uxTaskPriorityGet需设1 |
| xTaskGetTickCount() | task.h | 读取调度器启动后的Tick计数 | 超时/周期诊断 | 否；ISR用xTaskGetTickCountFromISR | 是，诊断已用 |
| xTaskGetSchedulerState() | task.h | 判断未启动/运行/挂起 | 验证GUI进入调度器 | 当前单核非MPU实现可只读查询，见下文例外 | 是；INCLUDE_xTaskGetSchedulerState=1 |
| uxTaskGetStackHighWaterMark() | task.h | 返回历史最小未使用栈，单位为字 | 检查GUI/Input任务栈余量 | 否，扫描栈应放任务上下文 | 是；对应INCLUDE宏=1 |

任务优先级与中断优先级的数字方向相反：任务3比2更优先；中断4比5更优先。不要混淆两套概念。

### 8.2 队列

| API 函数 | 所属头文件 | 作用 | 使用场景 | 能否在中断中调用 | 当前工程是否可用 |
| --- | --- | --- | --- | --- | --- |
| xQueueCreate() | queue.h | 创建固定元素大小的队列 | 创建Input→GUI事件通道 | 否 | 是；动态分配已开，当前无实例 |
| xQueueSend() | queue.h | 向队尾发送一份数据 | Input_Task发送按键消息 | 否，等待0也不可以 | 是，尚未调用 |
| xQueueSendToBack() | queue.h | 显式发送到队尾 | 与xQueueSend等价，突出FIFO | 否 | 是，尚未调用 |
| xQueueSendFromISR() | queue.h | ISR中向队尾发送数据，不阻塞 | 合法优先级EXTI发送简短事件 | 是，仅ISR | 是，尚未调用；TIM3/TIM4不能用 |
| xQueueReceive() | queue.h | 取出并删除队头元素 | GUI消费输入事件 | 否；ISR用xQueueReceiveFromISR | 是，尚未调用 |
| xQueuePeek() | queue.h | 复制队头但不删除 | 查看待处理事件 | 否；ISR用xQueuePeekFromISR | 是，尚未调用 |
| uxQueueMessagesWaiting() | queue.h | 查询当前队列元素数量 | 观察积压 | 否；ISR用uxQueueMessagesWaitingFromISR | 是，尚未调用 |

队列按创建时的 `uxItemSize` **复制数据**。发送指针只会复制指针值，不会自动复制指向的整段内存。

### 8.3 信号量与互斥量

| API 函数 | 所属头文件 | 作用 | 使用场景 | 能否在中断中调用 | 当前工程是否可用 |
| --- | --- | --- | --- | --- | --- |
| xSemaphoreCreateBinary() | semphr.h | 创建初值为空的二值信号量 | 输入事件唤醒，不携带按键数据 | 否 | 是，依托queue.c；当前无实例 |
| xSemaphoreCreateCounting() | semphr.h | 创建有上限的计数信号量 | 累计事件或资源数量 | 否 | 否；USE_COUNTING_SEMAPHORES需设1 |
| xSemaphoreCreateMutex() | semphr.h | 创建带优先级继承的普通互斥量 | 多任务确实需要共享同一资源时 | 否 | 否；USE_MUTEXES需设1 |
| xSemaphoreCreateRecursiveMutex() | semphr.h | 创建允许同任务重复持有的互斥量 | 共享资源接口存在同任务嵌套调用 | 否 | 否；USE_MUTEXES和USE_RECURSIVE_MUTEXES均需1 |
| xSemaphoreTake() | semphr.h | 获取二值/计数信号量或普通互斥量，可等待 | 等事件、保护共享资源 | 否；ISR专用take仅限信号量且不等待 | 二值可；计数/互斥对象需先启用创建功能 |
| xSemaphoreGive() | semphr.h | 释放信号量或普通互斥量 | 任务通知事件、归还资源锁 | 否；ISR用GiveFromISR | 二值可；其他对象按配置 |
| xSemaphoreGiveFromISR() | semphr.h | ISR释放二值/计数信号量 | 合法优先级ISR唤醒Input_Task | 是；**禁止用于互斥量** | 二值可；计数需先开计数功能 |
| xSemaphoreTakeRecursive() | semphr.h | 递归获取，记录持有次数 | 同一任务层层调用同一资源接口 | 否 | 否；需开启递归和普通互斥量 |
| xSemaphoreGiveRecursive() | semphr.h | 递归释放，次数与take匹配 | 从嵌套资源接口返回 | 否 | 否；需开启递归和普通互斥量 |

二值信号量用于“有事件”，没有资源所有者和优先级继承；互斥量用于“谁拥有共享资源”，由持有任务释放，不能当ISR通知工具。为了NWatch安全，优先维持GUI单独拥有OLED/绘图状态，避免为了并发而让多个任务直接画图。

### 8.4 事件组

| API 函数 | 所属头文件 | 作用 | 使用场景 | 能否在中断中调用 | 当前工程是否可用 |
| --- | --- | --- | --- | --- | --- |
| xEventGroupCreate() | event_groups.h | 创建事件位集合 | 表示输入/传感器/显示就绪 | 否 | 否；需加入event_groups.c，USE_EVENT_GROUPS保持1 |
| xEventGroupSetBits() | event_groups.h | 任务中置一个或多个位 | Input_Task报告某条件满足 | 否 | 否；同上 |
| xEventGroupSetBitsFromISR() | event_groups.h | 把置位请求送给定时器服务任务 | ISR报告事件条件 | 是，但通过守护任务延后处理 | 否；另需timers.c、USE_TIMERS=1、INCLUDE_xTimerPendFunctionCall=1与定时器配置 |
| xEventGroupWaitBits() | event_groups.h | 等任一/全部事件位，可返回时清位 | 多个初始化步骤完成再开始业务 | 否，即使等待0也不能在ISR用 | 否；需加入event_groups.c |
| xEventGroupClearBits() | event_groups.h | 清除事件位 | 复位状态条件 | 否；另有ClearBitsFromISR | 否；需加入event_groups.c |
| xEventGroupGetBits() | event_groups.h | 读取当前事件位，宏封装ClearBits(...,0) | 状态查询 | 否；ISR用xEventGroupGetBitsFromISR | 否；需加入event_groups.c |

当前32位Tick配置下，事件组可给应用使用**低24位**；高8位内核保留。事件位不保存事件发生次数，也不适合代替完整按键消息队列。

### 8.5 任务通知

以下API都属于 `task.h`；当前 `configUSE_TASK_NOTIFICATIONS=0`，因此**均不可用**。以后设1即可由现有tasks.c提供，不要另找“notify.c”。

| API 函数 | 所属头文件 | 作用 | 使用场景 | 能否在中断中调用 | 当前工程是否可用 |
| --- | --- | --- | --- | --- | --- |
| xTaskNotifyGive() | task.h | 给目标任务通知值加1 | 任务间轻量唤醒 | 否 | 否；需USE_TASK_NOTIFICATIONS=1 |
| ulTaskNotifyTake() | task.h | 等通知计数并清零/减1 | Input_Task等待采样事件 | 否 | 否；同上 |
| xTaskNotify() | task.h | 以位、数值、覆盖等动作通知目标 | 传少量状态或标志 | 否 | 否；同上 |
| xTaskNotifyWait() | task.h | 等待通知并读取数值/位 | 任务等待多个通知位 | 否 | 否；同上 |
| vTaskNotifyGiveFromISR() | task.h | ISR给目标任务通知值加1 | EXTI唤醒唯一Input_Task | 是，仅ISR | 否；同上及合法IRQ优先级 |
| xTaskNotifyFromISR() | task.h | ISR按动作更新通知 | ISR上报标志位/小数值 | 是，仅ISR | 否；同上及合法IRQ优先级 |
| xTaskNotifyStateClear() | task.h | 清“通知待处理”状态，保留通知值 | 明确通知状态机后清理挂起状态 | 否 | 否；需USE_TASK_NOTIFICATIONS=1 |

任务通知是发给**一个已知任务**的轻量机制；不是多人读取的广播队列。默认未索引版本用索引0，未来流/消息缓冲也会使用任务通知，要设计好用途与索引。

### 8.6 内存管理与运行状态

| API 函数 | 所属头文件 | 作用 | 使用场景 | 能否在中断中调用 | 当前工程是否可用 |
| --- | --- | --- | --- | --- | --- |
| pvPortMalloc() | portable.h，经FreeRTOS.h引入 | 从RTOS堆分配字节 | 内核创建任务/队列，必要的动态缓冲 | 否 | 是，heap_4已选中 |
| vPortFree() | portable.h，经FreeRTOS.h引入 | 释放RTOS堆内存 | 释放不再使用的动态缓冲 | 否 | 是；当前业务未直接使用 |
| xPortGetFreeHeapSize() | portable.h，经FreeRTOS.h引入 | 返回RTOS堆当前剩余总字节 | 新任务/队列前后评估余量 | 否，保持在任务中采样 | 是，已用 |
| xPortGetMinimumEverFreeHeapSize() | portable.h，经FreeRTOS.h引入 | 返回历史最小剩余总字节 | 捕捉最坏路径的堆压力 | 否，保持在任务中采样 | 是，已用 |
| uxTaskGetNumberOfTasks() | task.h | 返回受内核管理的任务数，包括Idle | 确认任务是否创建 | 当前单核实现为只读计数；应用建议在任务中使用 | 是，诊断已用，既有实测2 |

只读例外说明：本地V11.3.1的 `xTaskGetSchedulerState` 只读取调度状态，`uxTaskGetNumberOfTasks` 只返回计数；当前单核、非MPU端口可作ISR只读快照。这是基于**当前实现的核对**，不是可推广到其他API/MPU/SMP端口的授权。普通任务、队列、通知API仍严格遵循FromISR边界。官方也曾将调度器状态的ISR可用性说明为实现上的特例：[官方维护者说明](https://www.freertos.org/FreeRTOS_Support_Forum_Archive/January_2013/freertos_xTaskGetSchedulerState_API_and_isr_6699880.html)。

### 8.7 软件定时器

| API 函数 | 所属头文件 | 作用 | 使用场景 | 能否在中断中调用 | 当前工程是否可用 |
| --- | --- | --- | --- | --- | --- |
| xTimerCreate() | timers.h | 创建定时器对象，不自动启动 | 后续提示/超时动作 | 否 | 否；TIMERS=0且timers.c未构建 |
| xTimerStart() | timers.h | 发送启动命令 | 启动到期回调 | 否；ISR用xTimerStartFromISR | 否；需启用完整定时器服务 |
| xTimerStop() | timers.h | 发送停止命令 | 取消提示/超时 | 否；ISR用xTimerStopFromISR | 否；同上 |
| xTimerReset() | timers.h | 从重置时刻重新计时，未运行时也可启动 | “最后一次按键后N秒”类超时 | 否；ISR用xTimerResetFromISR | 否；同上 |

软件定时器与TIM3/TIM4硬件定时器是两件事；当前硬件时基运行正常，并不表示FreeRTOS软件定时器已启用。

## 9. 重点API：原型、参数、返回值、错误和NWatch用途

以下原型省略 `PRIVILEGED_FUNCTION` 等链接修饰以方便阅读。若API实际是宏，会明确注明“调用形式”，并不是声称存在同名独立链接函数。

### 9.1 任务管理重点

#### xTaskCreate —— 创建任务

~~~c
BaseType_t xTaskCreate(
    TaskFunction_t pxTaskCode,
    const char * const pcName,
    const configSTACK_DEPTH_TYPE uxStackDepth,
    void * const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t * const pxCreatedTask);
~~~

- 参数：入口必须形如 `void Task(void *argument)`；名字仅便于识别；栈深度以 `StackType_t` 元素为单位；pvParameters是原样传入的指针；优先级本工程取0～3；最后一项用于输出任务句柄，确实不需要时可以NULL。
- 返回：成功 `pdPASS`，分配失败 `errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY`。当前malloc失败钩子可能在返回前进入故障停机，因此还需观察g_nwatchFault。
- 本工程 `StackType_t` 是32位，768表示3072B，**不是768B**。名字数组配置12字节，能完整保存的名称最多11字符加结束符。
- 常见错：任务函数直接return；把短生命周期局部变量地址当长期参数；不保存/检查创建结果；把栈单位当字节；给越界优先级。
- NWatch：rtos_app.c已经创建名为NWatchGUI、栈768字、优先级2的任务。以后Input_Task也在这一层创建，先创建它依赖的队列。

#### vTaskDelay —— 相对延时

~~~c
void vTaskDelay(const TickType_t xTicksToDelay);
~~~

- 参数：相对调用时刻等待的Tick数，通常用 `pdMS_TO_TICKS(ms)` 换算。当前1000Hz时1 Tick对应1ms。
- 返回：无。
- 常见错：把数值直接当通用毫秒；用它保证严格固定周期；在ISR、临界区或挂起调度器时调用；用vTaskDelay(0)期待真正睡眠。
- NWatch：GUI每轮 `vTaskDelay(pdMS_TO_TICKS(1U))`，阻塞后Idle有机会运行。以后Input轮询可先用相对延时，但周期会叠加工作时间。
- 细节：延时精度受Tick边界和调度影响。等待1 Tick可能小于完整1ms；重新就绪也不等于立刻获得CPU。

#### xTaskDelayUntil / vTaskDelayUntil —— 固定节拍

~~~c
BaseType_t xTaskDelayUntil(
    TickType_t * const pxPreviousWakeTime,
    const TickType_t xTimeIncrement);

/* 头文件真实存在的旧形式宏，内部忽略xTaskDelayUntil的返回值。 */
#define vTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement) /* 宏 */
~~~

- 参数：上一目标唤醒时刻的地址；每次推进的周期Tick数（必须大于0）。循环前用 `xTaskGetTickCount()` 初始化该变量，函数每次会更新它。
- 返回：xTaskDelayUntil返回pdTRUE表示本次实际进入延时，pdFALSE表示目标已错过、没有阻塞。旧v形式丢弃返回值，不提供同名独立实现。
- 当前不可用：`INCLUDE_xTaskDelayUntil=0`。设为1以后可用；不要新增旧的 `INCLUDE_vTaskDelayUntil`，V11.3.1还会对同时定义新旧配置报错。
- 常见错：每轮重新初始化上一唤醒时刻，退化成相对延时；周期换算成0；工作耗时大于周期却仍以为保证准时；误把v形式当当前独立函数。
- NWatch：适合未来Input_Task固定频率读取已解码输入。当前GUI仍由原显示时基控帧，不应只因学习此API就替换现有GUI循环。
- 本地task.h无条件保留v形式宏，**即使configENABLE_BACKWARD_COMPATIBILITY=0也存在**；但底层x实现关闭时，两者都不能有效链接。以本地task.h/tasks.c为准，而不是旧教程的函数列表。

#### uxTaskGetStackHighWaterMark —— 任务栈历史余量

~~~c
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask);
~~~

- 参数：要检查的任务句柄；NULL表示当前任务。
- 返回：自任务创建以来观察到的最小未使用栈元素数；在此端口乘4才是字节。
- 常见错：把返回数当已用栈或字节；当成当前瞬时剩余；只走空闲路径后就缩栈；在高频ISR扫描栈；使用无效/已删除句柄。
- NWatch：updateDiagnostics同时采GUI和Idle；以后新增Input后可按同一方式低频采样。新调用路径、printf、较大局部数组都会改变水位。

#### 其他任务查询/控制参数速记

| API调用形式 | 参数与返回 | 易错点 |
| --- | --- | --- |
| void vTaskDelete(TaskHandle_t task) | task=NULL删除自己；无返回值 | 当前关闭；自删后Idle负责回收内核分配的栈/TCB；业务申请的缓冲、锁不能指望自动清理 |
| void vTaskPrioritySet(TaskHandle_t task, UBaseType_t priority) | NULL为自己；无返回值 | 当前关闭；优先级仍须0～3，修改可能马上触发抢占 |
| UBaseType_t uxTaskPriorityGet(const TaskHandle_t task) | NULL为自己；返回当前优先级 | 当前关闭；不是中断优先级 |
| TickType_t xTaskGetTickCount(void) | 无参数；返回内核Tick | 已开；ISR改用FromISR版；使用无符号差值处理回绕 |
| BaseType_t xTaskGetSchedulerState(void) | 返回taskSCHEDULER_NOT_STARTED/RUNNING/SUSPENDED | 已开；当前常量分别1/2/0，代码应比较名字；不是CPU占用率 |

### 9.2 队列重点

#### xQueueCreate —— 创建固定大小消息队列

~~~c
/* 宏的类型化调用形式，实际展开为xQueueGenericCreate。 */
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength,
                          UBaseType_t uxItemSize);
~~~

- 参数：最多容纳多少个元素；每个元素的字节数。传 `sizeof(InputEvent)`，不是总缓存字节数。
- 返回：队列句柄；堆不足时NULL（当前失败钩子可能先停机）。
- 常见错：元素大小与收发结构不一致，导致越界或截断；每次循环重新创建队列；对象创建失败仍传给其他API；先启动生产者再初始化句柄。
- NWatch：未来在调度启动前建立Input→GUI通道。队列只保存事件结构，不保存整帧OLED画面。
- 当前可用：queue.c和动态分配均已具备，不需要开启互斥量或任务通知。

#### xQueueSend / xQueueSendToBack —— 任务发送

~~~c
/* 二者都是宏，均展开为xQueueGenericSend(..., queueSEND_TO_BACK)。 */
BaseType_t xQueueSend(QueueHandle_t xQueue,
                     const void * pvItemToQueue,
                     TickType_t xTicksToWait);
BaseType_t xQueueSendToBack(QueueHandle_t xQueue,
                           const void * pvItemToQueue,
                           TickType_t xTicksToWait);
~~~

- 参数：有效队列；要复制的元素地址；队列满时最多等待多少Tick，0表示立即尝试。
- 返回：pdPASS成功，errQUEUE_FULL失败/超时。失败不是“自动排队稍后重试”。
- 常见错：传结构值而不是地址；发送局部缓冲的指针后原缓冲失效；GUI长时间等满队列导致界面停顿；在ISR调用普通发送函数。
- NWatch：Input_Task发送小型“按键类型/按键码/时间戳”结构。应事先决定队满时丢弃、合并重复事件还是报告计数；不能默默忽略丢失。

#### xQueueReceive —— 任务接收

~~~c
BaseType_t xQueueReceive(QueueHandle_t xQueue,
                        void * const pvBuffer,
                        TickType_t xTicksToWait);
~~~

- 参数：队列；可容纳一个完整元素的输出缓冲区；队列空时的最长等待Tick。
- 返回：pdPASS取得一个元素并移除；errQUEUE_EMPTY失败/超时。
- 常见错：接收缓冲太小；读取失败仍处理上次旧值；先查询数量再接收并假设绝不会失败；GUI使用无限/很长等待，停止动画刷新。
- NWatch：GUI应在现有循环的明确输入接入点，使用0或受控的很短等待，并限制每轮处理数量，避免队列洪水挤占绘图时间。连续动画期间不能靠“永远等按键”驱动整个GUI。

#### xQueueSendFromISR —— ISR发送的完整模式

~~~c
/* 宏，展开为xQueueGenericSendFromISR。 */
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue,
                            const void * pvItemToQueue,
                            BaseType_t * pxHigherPriorityTaskWoken);
~~~

- 参数：队列、要复制的元素地址、是否唤醒了更高优先级任务的输出标志。
- 返回：pdPASS或errQUEUE_FULL。它不会阻塞，也没有等待Tick参数。
- 每次ISR先 `BaseType_t woken = pdFALSE;`，把 `&woken` 传入，处理完成后 `portYIELD_FROM_ISR(woken);`。同一次ISR多次调用可以共用标志，不要中途复位。
- 常见错：忘记初始化woken；用返回pdPASS代替woken请求切换；忘记处理队满；在优先级4的TIM3/TIM4中使用；中断可能在队列句柄创建前就进来。
- NWatch：EXTI15_10数字优先级5符合阈值，未来可考虑发送简短事件；仍需处理上电初始化阶段和生产者唯一性。本轮不向现有红外ISR添加调用。

补充：`xQueuePeek(queue, &event, wait)`返回规则与Receive类似，但元素保留；`uxQueueMessagesWaiting(queue)`返回数量快照，不能替代Receive返回值检查。

### 9.3 信号量和互斥量重点

#### xSemaphoreCreateBinary —— 创建空二值信号量

~~~c
/* 宏，底层是长度1、元素长度0的特殊队列。 */
SemaphoreHandle_t xSemaphoreCreateBinary(void);
~~~

- 参数：无。
- 返回：有效句柄或NULL。**创建成功时没有可取令牌**；与某些旧教程的vSemaphoreCreateBinary初态不同。
- 常见错：以为创建完第一次take必然成功；使用二值信号量保存多个事件次数；用它代替带数据的队列。
- NWatch：适合ISR告诉Input_Task“有新输入可读”。若需要保留每个键码和先后顺序，优先使用队列。

#### xSemaphoreTake —— 取令牌/普通锁

~~~c
/* 宏，展开为xQueueSemaphoreTake。 */
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore,
                         TickType_t xBlockTime);
~~~

- 参数：二值/计数信号量或普通互斥量句柄；最长等待Tick，0立即尝试。
- 返回：pdTRUE取得，pdFALSE未取得/超时。
- 常见错：失败后仍访问共享资源；持锁后忘give；持锁进行长时间绘图/延时；递归互斥量错用普通take；在ISR阻塞。
- NWatch：未来Input_Task可等二值信号量，醒来读取输入。若以后确需共享I2C普通互斥量，必须先启用USE_MUTEXES并明确总线访问边界。
- 当前二值信号量可用；没有创建普通互斥量的功能，不应凭Take宏可见就认为Mutex已启用。

#### xSemaphoreGiveFromISR —— 中断唤醒任务

~~~c
/* 宏，展开为xQueueGiveFromISR。 */
BaseType_t xSemaphoreGiveFromISR(
    SemaphoreHandle_t xSemaphore,
    BaseType_t * pxHigherPriorityTaskWoken);
~~~

- 参数：二值/计数信号量；woken标志地址。
- 返回：pdTRUE成功；errQUEUE_FULL表示已满（通常等价于pdFALSE），二值信号量已有令牌时不能再积累一个。
- 常见错：把Mutex传进来；把二值信号量当无限计数器；woken没初始化/没请求切换；在IRQ优先级0～4中调用。
- NWatch：未来EXTI15_10可用它唤醒单独的Input_Task；TIM3/TIM4保持不调用内核。
- 普通任务用 `xSemaphoreGive(sem)`；它不等待，返回pdTRUE/失败。普通互斥量必须由持有者give。

#### 互斥量与递归的关键规则

`xSemaphoreCreateMutex(void)`、`xSemaphoreCreateRecursiveMutex(void)` 均返回句柄或NULL。普通互斥量创建后处于可获取状态，并有优先级继承。递归版的 `xSemaphoreTakeRecursive(mutex, wait)`、`xSemaphoreGiveRecursive(mutex)` 返回pdTRUE/pdFALSE；同一任务take几次就要give几次，不能混用普通take/give。四个相关API当前都要先启用配置。

### 9.4 事件组重点（以下均须先加入源码）

#### xEventGroupCreate

~~~c
EventGroupHandle_t xEventGroupCreate(void);
~~~

- 参数：无。返回新句柄或NULL，创建后应用事件位全0。
- 常见错：只include头文件却没有编译event_groups.c；误以为事件位包含次数/数据。
- NWatch：以后需要等待“输入初始化完成”和“某外设就绪”等多条件时使用。简单按键传递不必因此更换Queue。

#### xEventGroupWaitBits

~~~c
EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToWaitFor,
    const BaseType_t xClearOnExit,
    const BaseType_t xWaitForAllBits,
    TickType_t xTicksToWait);
~~~

- 参数：事件组；要等的非零位掩码；满足后是否自动清所等待的位；pdTRUE等全部、pdFALSE等任一；最长等待Tick。
- 返回：解除等待时的事件位快照，成功清位场景返回的是清位前值。**不是pdPASS布尔值**，超时也返回位图，要自己按掩码判断任一/全部条件。
- 常见错：把返回非零当作全部满足；使用高8位；ISR调用；认为一次置位只唤醒一个等待者；退出后再手动清位造成与生产者竞态。
- NWatch：可协调初始化阶段；GUI持续动画时不宜长时间阻塞在“等任意输入位”。

#### xEventGroupSetBitsFromISR

~~~c
BaseType_t xEventGroupSetBitsFromISR(
    EventGroupHandle_t xEventGroup,
    const EventBits_t uxBitsToSet,
    BaseType_t * pxHigherPriorityTaskWoken);
~~~

- 参数：事件组、置位掩码、woken输出。
- 返回：pdPASS表示延后执行请求进入定时器命令队列，pdFAIL表示队列满等提交失败。**成功不保证ISR返回前已经完成置位**。
- 原因：置位可能解除不定数量的任务等待，操作延后到定时器服务任务执行。woken可能指向唤醒服务任务，而不是最终的事件等待者。
- 当前不可用：event_groups.c未编译，USE_TIMERS=0，INCLUDE_xTimerPendFunctionCall默认0；三层条件都要处理。
- 常见错：仅把USE_EVENT_GROUPS设1；按普通SetBits返回位图理解；忘记检查定时器命令队列满；事件组对象已释放但延后命令还引用它。
- NWatch：本项目目前更适合ISR→Queue或二值信号量，之后有多条件广播需求再评估事件组服务开销。

补充：任务版 `EventBits_t xEventGroupSetBits(group, bits)` 返回调用结束时位图，可能受等待任务自动清位影响；`xEventGroupClearBits(group,bits)` 返回清位前位图；`xEventGroupGetBits(group)`是ClearBits(group,0)宏，不可在ISR用。ISR读取应选GetBitsFromISR。

### 9.5 任务通知重点（以下均须先开启通知）

#### xTaskNotifyGive + ulTaskNotifyTake —— 轻量事件计数

~~~c
/* 都是宏的类型化调用形式。 */
BaseType_t xTaskNotifyGive(TaskHandle_t xTaskToNotify);
uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit,
                         TickType_t xTicksToWait);
~~~

- Give参数是**目标任务**句柄；内部使用eIncrement。返回pdPASS；必须保证目标句柄有效，不会帮你创建任务。
- Take只能等待**调用任务自己**的通知：pdTRUE在退出时把计数清零；pdFALSE消费一个计数；第二项是最长等待Tick。
- Take返回消费/清零前的通知计数；0通常表示等到超时仍没有计数。
- 常见错：把信号量句柄传给通知；想让一个通知广播唤醒多个任务；每来一次就必须逐个处理却用pdTRUE丢掉累计次数；任务尚未创建就由ISR发送。
- NWatch：以后已知唯一Input_Task时，可以用通知减少对象开销；但通知本身不保留每次按键的完整结构。

#### xTaskNotifyWait —— 等位/数值通知

~~~c
/* 宏的类型化调用形式。 */
BaseType_t xTaskNotifyWait(
    uint32_t ulBitsToClearOnEntry,
    uint32_t ulBitsToClearOnExit,
    uint32_t * pulNotificationValue,
    TickType_t xTicksToWait);
~~~

- 参数：在进入等待时要清除的通知位；成功退出时要清的位；输出通知值指针（不用可NULL）；等待Tick。
- 进入清位发生在没有待处理通知的路径；已有通知待处理时不会把它简单丢弃。退出成功时先取得输出快照，再清指定的位。
- 返回：pdTRUE收到通知，pdFALSE超时。它关注“通知到达状态”，与ulTaskNotifyTake关注非零计数的语义不同。
- 常见错：混用计数和位图协议；把输出指针当输入掩码；依赖它排队保存多笔覆盖式数据。
- NWatch：将来Input_Task可以按位接收“需要采样/需要复位”等控制指令；按键数据仍交由Queue传递。

#### xTaskNotifyFromISR —— 按动作通知任务

~~~c
/* 宏的类型化调用形式。 */
BaseType_t xTaskNotifyFromISR(
    TaskHandle_t xTaskToNotify,
    uint32_t ulValue,
    eNotifyAction eAction,
    BaseType_t * pxHigherPriorityTaskWoken);
~~~

- 参数：目标任务；传入的32位数值；更新动作；woken标志。
- 动作：eSetBits按位或；eIncrement加1（忽略ulValue）；eSetValueWithOverwrite覆盖；eSetValueWithoutOverwrite只在无待处理通知时写入；eNoAction只改变通知状态。
- 返回：通常pdPASS；eSetValueWithoutOverwrite遇到尚未处理通知时pdFAIL。
- 常见错：使用覆盖动作却以为每次消息都保留；woken未初始化；任务句柄尚未有效；通知功能仍为0；普通任务误用FromISR版。
- NWatch：EXTI可按需报告少量状态，但完整输入事件建议Queue。只需要加1时用 `vTaskNotifyGiveFromISR(task, &woken)`，它是**void返回**，不要写成x开头。
- `xTaskNotify(task,value,action)`是对应任务版，返回规则相同。
- `BaseType_t xTaskNotifyStateClear(TaskHandle_t task)`：NULL为当前任务；原来有待处理通知时返回pdTRUE，否则pdFALSE；只清状态，不清通知值。不能把它当成“清空计数”。

### 9.6 内存管理重点

#### pvPortMalloc

~~~c
void * pvPortMalloc(size_t xWantedSize);
~~~

- 参数：请求字节数，不是字数。
- 返回：满足对齐要求的指针，失败返回NULL；当前malloc失败钩子会记录故障并停机，失败可能不返回应用。
- 常见错：ISR里分配；未判断NULL；误以为剩余总量足够就一定有连续大块；频繁申请释放造成不必要碎片；与C库free混用。
- NWatch：xTaskCreate/xQueueCreate内部已经分配栈、TCB或队列存储。业务层只在确有需要时直接申请，通常启动阶段一次创建对象更清楚。

#### vPortFree

~~~c
void vPortFree(void * pv);
~~~

- 参数：由pvPortMalloc家族返回、尚未释放的有效块首地址；NULL可忽略。
- 返回：无。
- 常见错：double free；释放数组内部地址或栈变量；释放后继续访问；队列里还有指向缓冲的消息就释放；用vPortFree释放malloc返回的C堆指针。
- NWatch：未来动态消息缓冲必须定义“发送成功后谁拥有、谁释放”；小输入事件按值入队，可以省去这个所有权问题。

#### xPortGetFreeHeapSize / xPortGetMinimumEverFreeHeapSize

~~~c
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
~~~

- 参数：均无。返回字节数，前者当前空闲总量，后者初始化以来历史最低空闲总量（除非以后显式重置统计）。
- 常见错：把它当全芯片RAM剩余；把历史最小当当前值；当作最大连续可分配块；创建后马上删除对象就以为从未用过堆。
- NWatch：现在由updateDiagnostics每秒保存到g_nwatchRtos，后续增加Input_Task和Queue前后比较这两个值，结合栈水位判断容量。
- 查询值不包含链接器未分配的9224B静态余量；那部分不会自动增长进当前6KiB的ucHeap。

`UBaseType_t uxTaskGetNumberOfTasks(void)`无参数，返回总任务数；当前既有实测2，开启软件定时器以后通常还会增加一个Timer Service任务。不能把任务数增加等同CPU负载增加的固定百分比。

### 9.7 软件定时器重点（当前均不可用）

#### xTimerCreate

~~~c
TimerHandle_t xTimerCreate(
    const char * const pcTimerName,
    const TickType_t xTimerPeriodInTicks,
    const BaseType_t xAutoReload,
    void * const pvTimerID,
    TimerCallbackFunction_t pxCallbackFunction);
/* 回调类型：void Callback(TimerHandle_t xTimer); */
~~~

- 参数：调试名；非零周期Tick；pdTRUE自动重装，pdFALSE一次性；用户ID/上下文；回调函数。
- 返回：定时器句柄或NULL。创建成功仍处于未启动状态。
- 常见错：以为创建就开始倒计时；周期=0；把回调当成硬件ISR；回调中长时间等待、绘图或调用阻塞API。
- NWatch：以后可用于提示到期/输入不活动超时。回调应该只发送短事件，GUI任务决定界面变化，继续独占绘图。

#### xTimerStart

~~~c
/* 宏的类型化调用形式。 */
BaseType_t xTimerStart(TimerHandle_t xTimer,
                      TickType_t xTicksToWait);
~~~

- 参数：定时器；向定时器命令队列发送命令时最多等多久，**不是定时器周期**。
- 返回：pdPASS表示命令成功提交；pdFAIL表示命令队列提交超时/失败，不保证返回时回调/状态改变已经发生。
- 常见错：忽略失败；把xTicksToWait当启动后延时；ISR用普通Start；在定时器回调里用非零阻塞时间提交命令造成服务任务自阻塞。
- NWatch：以后启动一个提示计时，回调仅给GUI送消息。先启用完整定时器服务并预留RAM。

#### xTimerReset / xTimerStop

~~~c
/* 均为宏的类型化调用形式。 */
BaseType_t xTimerReset(TimerHandle_t xTimer,
                      TickType_t xTicksToWait);
BaseType_t xTimerStop(TimerHandle_t xTimer,
                     TickType_t xTicksToWait);
~~~

- 参数：对象与命令提交等待Tick，含义同Start。
- 返回：pdPASS/pdFAIL只代表提交结果。
- Reset重新建立到期时刻，未运行的定时器也可以启动；Stop请求停止。命令异步处理，不能把Stop成功理解成已经执行中的回调被强制终止。
- 常见错：每次输入都Create新对象而非Reset；以为Stop自动释放对象；在回调中阻塞；从ISR调用普通版本。
- NWatch：最后一次按键后若干秒发出超时事件，可以重置同一个定时器；当前原pwrmgr行为保持原样，本文没有替换它。


## 10. ISR、中断优先级和等待时间的统一规则

### 10.1 Cortex-M3优先级阈值

本项目 `configPRIO_BITS=4`、`NVIC_PRIORITYGROUP_4`，使用4个抢占优先级位。

| 配置/中断 | 逻辑优先级（HAL/NVIC参数） | 寄存器编码 | 对RTOS通信API的含义 |
| --- | --- | --- | --- |
| configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY | 5 | configMAX_SYSCALL_INTERRUPT_PRIORITY=0x50 | 可调用FromISR内核服务的最高中断紧急程度 |
| TIM3微秒计时 | 4 | 0x40 | 比阈值更紧急，**不能调用Queue/Semaphore/Notify等FromISR服务** |
| TIM4 HAL时基 | 4 | 0x40 | 同上，保持仅HAL计时 |
| EXTI15_10红外 | 5 | 0x50 | 优先级符合FromISR条件；仍需对象/任务初始化完成 |
| PendSV / SysTick | 15 | 0xF0 | 端口的内核异常入口 |
| 其他外设ISR | 需实际检查 | 逻辑值左移4位 | 若要调用FromISR，应在5～15范围，并保持正确分组 |

STM32里数字越小，中断越紧急。优先级0～4能够打断FreeRTOS使用BASEPRI保护的部分临界区，所以不能接触队列/任务调度内部状态。把 `0x50` 直接传给HAL_NVIC_SetPriority是错的，HAL应传逻辑值5。

合法优先级只是必要条件。EXTI在GPIO初始化时就可能开启，而未来队列/任务可能更晚创建；不能让早到中断对NULL句柄发送。以后需要在相应对象准备好后再启用生产路径，或明确处理未就绪状态。本轮不改开启顺序。

### 10.2 标准的ISR唤醒形式（仅文档示意）

~~~c
/* 示意：前提为队列已创建、ISR优先级合法、生产路径已准备好。 */
BaseType_t higherPriorityTaskWoken = pdFALSE;
/* InputEvent event = ...; 只使用已确定的短消息。 */
if (xQueueSendFromISR(inputQueue, &event,
                      &higherPriorityTaskWoken) != pdPASS)
{
    /* 记录丢失计数；ISR不阻塞等待、不printf、不绘图。 */
}
portYIELD_FROM_ISR(higherPriorityTaskWoken);
~~~

这不是可以直接粘进当前ISR的完整代码，inputQueue、InputEvent与event均是未来设计的对象/类型。任务代码应调用普通xQueueSend；不要为了“方便”在任务中使用FromISR版。

### 10.3 不要误用portMAX_DELAY

当前 `INCLUDE_vTaskSuspend=0`。在可阻塞API中传 `portMAX_DELAY`，通常表示32位Tick的极长**有限**超时，而不是启用“永久等待”的专门路径。若未来确需永久阻塞语义，应核对 `INCLUDE_vTaskSuspend=1` 及该API实现。GUI动画循环则应避免无限等待输入。

不管等待多久，ISR都不能阻塞。FromISR函数没有阻塞等待参数；普通Receive/Take/WaitBits即使传0，也不能因此当作ISR接口。

### 10.4 Hook/回调在哪个上下文

| 回调/钩子 | 当前/未来执行上下文 | 主要边界 |
| --- | --- | --- |
| vApplicationTickHook | SysTick ISR，当前已启用 | 当前只累加计数；不能阻塞、绘图或调用普通API |
| vApplicationIdleHook | Idle任务，当前已启用 | 不能阻塞Idle；当前计数并WFI |
| vApplicationMallocFailedHook | 分配失败的调用路径 | 当前记录故障并停机；不要再分配/printf/OLED |
| vApplicationStackOverflowHook | 内核检查路径 | 当前记录故障；任务栈可能已坏，不做复杂处理 |
| TimerCallbackFunction_t | 未来的Timer Service任务 | 不是ISR，但不能长时间阻塞服务其他定时器；尽量发事件后立即返回 |
| HAL_GPIO_EXTI_Callback | 外设ISR调用链 | 与普通任务函数无关，调用名带Callback也不会变成任务上下文 |

## 11. RAM、FreeRTOS堆和任务栈

### 11.1 把目录与RAM关联起来

以下取自当前构建目录已有map的 `Image component sizes`，单位B，使用 `RW Data + ZI Data`，不是文件字节数/代码大小。既有总静态RAM为：

**11,256 / 20,480 B（约55.0%），链接范围剩余9,224 B。**

| 模块/文件 | 静态RAM | 解释 |
| --- | ---: | --- |
| heap_4.o | 6172 | ucHeap数组6144 + 分配器静态管理变量28 |
| tasks.o | 200 | 就绪链表、Tick/任务计数等；不含从ucHeap中分配的任务对象 |
| port.o | 12 | 端口状态 |
| list.o | 0 | 链表操作代码本身没有额外静态数据 |
| queue.o | 0 | 当前没有用户对象，函数被裁剪；未来对象还会从ucHeap占空间 |
| **FreeRTOS上述合计** | **6384** | 已含6KiB RTOS堆容器 |
| RTOS/rtos_app.o | 172 | 诊断结构96、故障结构68及句柄/采样变量等8 |
| startup_stm32f103xb.o | 1536 | MSP主栈1024 + C库堆512 |
| main.o | 84 | 当前I2C等句柄；GUI动态栈不记在这里 |
| NWatch/所有当前保留.c | 1348 | UI/动画/菜单等静态状态合计 |
| 其中watchface.o | 565 | 已包含于1348，绘图状态/缓冲等 |
| 其中tunemaker.o | 552 | 已包含于1348，当前演示状态/数据 |
| 其中buttons.o | 56 | 已包含于1348，输入状态 |
| 板级driver_*.o合计 | 1608 | 包含下面四项 |
| 其中driver_oled.o | 1032 | 帧缓冲1024 + 状态8 |
| 其中driver_ir_receiver.o | 424 | 红外时间戳/缓存/状态 |
| 其中driver_systick.o | 72 | TIM4时基句柄 |
| 其中driver_timer.o | 80 | TIM3句柄/溢出计数等 |
| HAL与中断相关对象静态数据 | 12 | 本次主要是HAL Tick相关变量 |
| system_stm32f1xx.o | 4 | SystemCoreClock |
| C库libspace.o | 96 | C库运行状态 |
| 链接填充 | 12 | 对齐等 |
| **总计（不重复相加“其中”行）** | **11256** | 对应map总RW+ZI |

资源文件 `resources.o` 在这份map中RAM为0、只读资源3145B；字体、位图、代码通常占Flash，不能因为源码文件大就认定占很多RAM。watchface.o另外有只读资源4416B，与其565B静态RAM是两种统计。

### 11.2 6KiB堆内部如何分配

既有 `acceptance/REPORT.md` 记录（本轮未新测）：

| 项目 | 数值 | 如何理解 |
| --- | ---: | --- |
| configTOTAL_HEAP_SIZE | 6144B | ucHeap整个数组，已在11256B中 |
| GUI栈 | 768字=3072B | 从ucHeap动态分配，不能再加到静态总RAM |
| Idle栈 | 128字=512B | 同样位于ucHeap |
| 两个任务栈配置合计 | 3584B | 这是保留的栈容量，不是历史实际用到的字节 |
| 当前/历史最小空闲堆 | 2384B / 2384B | 既有在线采样值 |
| 数组总量减空闲量 | 3760B | 包括两个栈以及TCB、分配块头、对齐/堆边界等 |
| 3760减3584 | 176B | 非栈部分的合计差额，不能全部当“应用又申请的缓冲” |
| GUI最低观测空余水位 | 686字=2744B | 已覆盖路径中观测到的历史最低；另一次短时采样688字 |
| Idle最低观测空余水位 | 110字=440B | 不能把它当当前瞬时使用深度 |

图解：

~~~text
20KiB芯片RAM
├─ 链接器已安排的RW+ZI：11256B
│  ├─ FreeRTOS ucHeap数组：6144B
│  │  ├─ GUI栈容量：3072B
│  │  ├─ Idle栈容量：512B
│  │  ├─ TCB/分配器块头/对齐/边界等：按既有快照差额合计176B
│  │  └─ heap_4可用空闲量：2384B（旧验收快照）
│  ├─ MSP主栈：1024B
│  ├─ C库堆：512B
│  └─ GUI全局变量/OLED缓冲/驱动/内核状态等
└─ 链接范围剩余：9224B
   这不是当前heap_4能够自动使用的空间
~~~

不能算 `11256 + 3072 + 512 + 6144`；这样把同一块ucHeap里的内容重复记账。

### 11.3 栈的三个单位

- xTaskCreate栈深度、configMINIMAL_STACK_SIZE：**StackType_t元素数（本项目32位字）**。
- uxTaskGetStackHighWaterMark返回值：**字**。
- pvPortMalloc、xPortGetFreeHeapSize、configTOTAL_HEAP_SIZE和startup的Stack_Size/Heap_Size：**字节**。

MSP与每个任务的PSP栈分别保留。单任务工作时任务栈要覆盖自己的最深调用链、局部变量、保存现场；中断嵌套还会用MSP。局部数组、格式化打印、浮点库路径都会使栈压力变化。

栈水位测的是填充模式保留下来的未使用区域，不是形式证明。当前GUI水位大，只说明既有覆盖路径用栈较少，**不构成本轮调整任务栈的依据**。本文没有改变任何栈值。

### 11.4 未来Input_Task和Queue如何估算

先使用式子估预算，不在本文替工程决定新栈大小：

~~~text
新增堆需求 ≈ Input_Task栈字数 × 4
          + Input_Task的TCB及分配开销
          + 队列控制结构
          + 队列长度 × 每个消息的sizeof
          + 相关块头/对齐
~~~

比如仅队列有效载荷若为8个、每个8B，就是64B；**不是说整个队列只占64B**，还要控制结构和分配开销。`sizeof(Queue_t)`、TCB大小会随配置改变，不能套网上固定值。

当前已有快照只剩2384B的heap_4空闲量，新任务栈很可能是主要新增项。9224B静态余量与2384B堆余量不是可以直接相加用于xTaskCreate的容量。未来另行评估，当前不扩堆、不缩GUI栈。

heap_4会合并相邻空闲块，但不表示完全没有碎片；空闲总量也不表示最大连续块。动态对象最好在启动阶段有计划地创建，避免把每帧/每次按键处理都变成申请释放循环。


### 11.5 与刚完成的RAM专项报告对照

更完整的地址布局与分布图见[20KB RAM分析报告](../ram_analysis/RAM_ANALYSIS.md)及[RAM分布图](../ram_analysis/ram_20kb_distribution.svg)。本文只引用其内存证据，后续容量调整不在本轮范围内。

两份文档的分组口径不同：本节按链接对象文件分组，RAM专项报告按用途分组。比如：

- 本文NWatch静态1348B + driver_oled静态1032B = 报告的OLED/UI合计2380B。
- 本文rtos_app.o为172B，其中诊断/故障结构164B，另有句柄/采样变量8B。按用途统计时，这8B可以并入内核管理项，不能再重复加。
- 当前TCB大小64B已由acceptance/verification.json核对；以后开启通知、Mutex等配置，TCB可能增大。

结合heap_4的8B块头、8B对齐和既有地址记录，可以将6KiB进一步分清：

~~~text
6144 = 3584（两个栈）
     + 128（两个64B TCB）
     + 32（四个已分配块头）
     + 2384（空闲块总量，含它自己的块头）
     + 16（首尾对齐及尾哨兵）
~~~

因此，`6144-2384=3760` 是“数组中不计入空闲量的部分”；真正已分配给任务及其块头的是3744B，其余16B是边界/对齐。2384B也不是单次有效载荷上限：在这个只有一个空闲块的既有快照里，扣8B块头后单次最大申请约2376B；将来有碎片时必须重新看最大连续块。

## 12. 将来新增Input_Task和Queue的正确位置

以下是后续开发方案，**本轮未新增任务文件、队列对象或编译项**。

~~~text
MDK-ARM/RTOS/
├─ rtos_app.c / .h       现有：集中初始化通信对象、创建任务、启动调度与诊断
├─ input_task.c / .h     建议新增：采集/读取已解码输入，产生InputEvent
└─ app_queue.c / .h      建议新增：InputEvent、队列创建、通信接口与句柄所有权

Core/Src/main.c的GUI_Task
└─ 在明确定义的输入接入点消费事件，继续拥有NWatch状态和绘图
~~~

未来建议架构：

~~~mermaid
flowchart LR
    IRQ[红外EXTI / 当前硬件采样解析] --> Cache[驱动输入缓存]
    Cache --> Input[未来Input_Task<br/>唯一读取驱动输入的消费者]
    Input --> Q[InputEvent Queue<br/>按值复制短消息]
    Q --> GUI[GUI_Task<br/>每轮有界地接收与分发]
    GUI --> State[NWatch按键动作/菜单状态]
    State --> OLED[原绘图与OLED接口]
~~~

实施顺序应是：

1. 在RTOS通信模块定义小而固定的消息结构，明确字段、队列长度和队满策略。
2. 在NWatch_RtosStart启动调度前初始化队列，检查创建结果。
3. 创建Input_Task、GUI等消费者/生产者任务，检查每次返回；确保任何ISR发送路径只在对象有效后启用。
4. Input_Task负责读输入并发送数据，不调用display/draw/menu/OLED。
5. GUI在现有循环中以非阻塞/短等待、有界批量方式消费事件，再分发按键动作，保留动画刷新节奏。
6. 对接 `NWatch/buttons.c` 的输入边界：当前buttons_update会读取红外并触发GUI回调；拆分时必须让驱动输入只有一个消费者，不能Input_Task与原buttons轮询同时调用IRReceiver_Read抢走数据。将来可做小型输入适配接口，GUI仍执行按钮动作。
7. 新.c需要加入Keil与EIDE的现行源文件列表；只把文件放到RTOS目录不会自动参与编译。
8. 再检查RAM、栈水位、队列积压/丢失、GUI帧间隔、中断优先级。这些是后续开发的验证步骤，本轮没有运行。

选择通信机制：

| 需求 | 优先考虑 | 原因 |
| --- | --- | --- |
| 每个按键码、先后顺序和时间戳 | Queue | 保存多笔完整的小消息 |
| 只需知道“有数据了”，实际数据在驱动缓存 | 二值信号量 | 当前配置可用，但合并重复唤醒 |
| 唤醒唯一目标且希望省独立对象 | 任务通知 | 轻量；当前需先开启 |
| 等多个状态条件 | 事件组 | 用位图组合条件；需要新增源码 |
| 多任务共享确实无法单独归属的资源 | 互斥量 | 有所有者和优先级继承；当前需先开启 |

没有必要为学习API一次性打开所有功能。Input_Task→Queue→GUI_Task这条主线，当前内核和队列基础已经具备；需要新增应用代码和评估RAM。固定周期的xTaskDelayUntil则需要后续单独打开其INCLUDE宏。

## 13. 最值得先掌握的15个函数

这份顺序围绕本项目的任务创建、输入队列与现有诊断，而不是官方API的全部重要程度。

| 顺序 | API | 先学会什么 | 当前状态 |
| --- | --- | --- | --- |
| 1 | xTaskCreate | 任务入口、参数、句柄、字单位栈 | 已用 |
| 2 | vTaskDelay | 让任务真正阻塞、让出CPU | 已用 |
| 3 | xTaskDelayUntil | 固定节拍与超期返回 | 需打开INCLUDE宏 |
| 4 | xTaskGetTickCount | 超时计时和回绕 | 已用 |
| 5 | xTaskGetSchedulerState | 调度器是否运行 | 已用 |
| 6 | uxTaskGetStackHighWaterMark | 历史栈余量及字节换算 | 已用 |
| 7 | xQueueCreate | 长度、元素大小和创建失败 | 可用，未建队列 |
| 8 | xQueueSend | 任务发送、复制与队满 | 可用，未调用 |
| 9 | xQueueSendFromISR | ISR边界、woken和优先级 | 可用，未调用 |
| 10 | xQueueReceive | 接收返回值与GUI短等待 | 可用，未调用 |
| 11 | xSemaphoreCreateBinary | 空初值、事件合并 | 可用，未创建 |
| 12 | xSemaphoreTake | 等事件/获取锁、超时处理 | 二值可用 |
| 13 | xSemaphoreGiveFromISR | ISR唤醒任务，不能释放互斥量 | 二值可用 |
| 14 | xPortGetFreeHeapSize | 当前RTOS堆字节余量 | 已用 |
| 15 | xPortGetMinimumEverFreeHeapSize | 运行历史最坏堆余量 | 已用 |

## 14. 最容易犯的10个错误

1. **把“目录里有”当作“已经编译”。** event_groups.c、timers.c都存在但没进构建；头文件有声明也不能保证实现已启用。
2. **栈单位混乱、RAM重复记账。** xTaskCreate传字，堆查询返回字节；GUI/Idle栈已在ucHeap内部，不能重复加到RW+ZI总量。
3. **在ISR调用普通API。** xQueueSend(...,0)仍不是ISR版；ISR禁止Delay/Receive等待、动态分配和绘图；任务通知也必须区分FromISR。
4. **只看FromISR名字，忽略优先级和初始化。** 本项目TIM3/TIM4为4，不能调用内核通信服务；EXTI=5也必须等队列/任务有效。
5. **忽略创建/发送/接收失败。** NULL句柄、队列满、接收超时都要处理；二值信号量不能累计任意次数；当前malloc失败还可能直接停机记录故障。
6. **把互斥量当ISR事件。** 互斥量由持有任务释放、有优先级继承；ISR只能按规定give信号量，不能GiveFromISR互斥量。
7. **让Input_Task直接绘图，或两个任务同时消耗红外缓存。** NWatch状态机/OLED应继续由GUI单独拥有；输入读取和事件转发要定义唯一所有者。
8. **队列元素大小/指针寿命错误。** sizeof必须一致；队列复制指针不会延长被指向栈变量的生命；发送成功与缓冲所有权要说清楚。
9. **用长时间阻塞拖住GUI/定时器服务。** GUI等按键不应停动画；Timer回调不做长耗时工作；portMAX_DELAY在当前配置不是永久阻塞专用语义；vTaskDelay不是固定周期保证。
10. **照搬旧教程或把采样值当绝对保证。** 当前首选xTaskDelayUntil，v形式是宏；通知/Mutex/Timer当前关闭；一次很高的栈水位不能证明所有路径安全，也不应据此立即改栈。

## 15. 本地核查入口与官方补充

后续源码变化后，应以本地配置重新核查，尤其是API开关与新.c是否加入两个工程：

- 本地 `RTOS/FreeRTOSConfig.h`：现行配置。
- 本地 `ThirdParty/FreeRTOS-Kernel/include/task.h`：版本号、任务/通知API、vTaskDelayUntil宏。
- 本地 `include/queue.h`、`semphr.h`：队列/信号量宏如何落到queue.c。
- 本地 `include/event_groups.h`、`event_groups.c`：事件位与ISR依赖。
- 本地 `include/timers.h`、`timers.c`：软件定时器命令和守护任务。
- 本地 `include/FreeRTOS.h`：未显式配置时的默认值。
- 本地 `portable/MemMang/heap_4.c`、`include/portable.h`：堆实现与内存API。
- 本地 `build/NWatch-DShanMCU-F103/builder.params`、同目录map与.obj/*.d：编译/依赖/链接依据。
- 本地 `acceptance/REPORT.md`：既有在线验收数据和测试覆盖限制。
- 本地 [ram_analysis/RAM_ANALYSIS.md](../ram_analysis/RAM_ANALYSIS.md)：刚完成的RAM专项分析、地址布局与分布图。

本地源码是本文版本相关结论的主要依据；需要对照发布来源时可查[官方V11.3.1发布页](https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/tag/V11.3.1)和[官方固定版本task.h](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.3.1/include/task.h)。API常规说明可参考[官方任务工具说明](https://compiler.freertos.org/Documentation/02-Kernel/04-API-references/03-Task-utilities/00-Task-utilities)，本项目的启用状态仍由本地配置决定。


## 附录 A. 官方源码包完整逐文件扫描清单

实际扫描到 **967 个文件**。下表每一个名称都来自本地文件系统，不是按标准目录模板补写。路径基准是 `MDK-ARM/ThirdParty/FreeRTOS-Kernel/`；将表中相对路径接在该基准之后就是完整工程路径。未选中平台只说明所属移植角色，不声称已经逐函数审计所有替代架构。

状态以现行48项sourceList及已有.d核对，头文件不单独编译。某文件被包含不代表其内部所有配置功能启用。官方包的示例/元数据明确列为支持资料，不能误认成我们自己编写的应用。


<details>
<summary>包根目录（22个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `.git-blame-ignore-revs` | 官方支持资料 官方源码包的构建/元数据/开发辅助文件 | 存在，不参与目标编译 |
| `.gitattributes` | 官方支持资料 官方源码包的构建/元数据/开发辅助文件 | 存在，不参与目标编译 |
| `CMakeLists.txt` | 官方支持资料 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `croutine.c` | 【官方内核】 旧式协程实现 | 存在，未加入当前构建 |
| `cspell.config.yaml` | 官方支持资料 官方源码包的构建/元数据/开发辅助文件 | 存在，不参与目标编译 |
| `event_groups.c` | 【官方内核】 事件位同步实现 | 存在，未加入当前构建 |
| `FreeRTOS-Kernel-V11.3.1-repository-SPDX2.3.spdx` | 官方支持资料 软件物料清单/许可元数据 | 存在，不参与目标编译 |
| `FreeRTOS-Kernel-V11.3.1-repository-SPDX2.3.spdx.json` | 官方支持资料 软件物料清单/许可元数据 | 存在，不参与目标编译 |
| `FreeRTOS-Kernel-V11.3.1-repository-SPDX2.3.spdx.xml` | 官方支持资料 软件物料清单/许可元数据 | 存在，不参与目标编译 |
| `FreeRTOS-Kernel-V11.3.1-repository-SPDX2.3.spdx.yaml` | 官方支持资料 软件物料清单/许可元数据 | 存在，不参与目标编译 |
| `GitHub-FreeRTOS-Kernel-Home.url` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `History.txt` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `LICENSE.md` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `list.c` | 【官方内核】 内核双向链表实现 | 已编译；有必要部分保留 |
| `manifest.yml` | 官方支持资料 官方源码包的构建/元数据/开发辅助文件 | 存在，不参与目标编译 |
| `MISRA.md` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `queue.c` | 【官方内核】 队列/信号量/互斥量基础实现 | 已编译；当前所有队列函数被裁剪 |
| `Quick_Start_Guide.url` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `README.md` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `stream_buffer.c` | 【官方内核】 流/消息缓冲实现 | 存在，未加入当前构建 |
| `tasks.c` | 【官方内核】 任务调度/创建/延时/通知（各API受配置裁剪） | 已编译；有必要部分保留 |
| `timers.c` | 【官方内核】 软件定时器服务任务及命令处理 | 存在，未加入当前构建 |

</details>

<details>
<summary>examples（9个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `examples/cmake_example/CMakeLists.txt` | 官方支持资料 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `examples/cmake_example/main.c` | 官方支持资料 官方示例源码/接口，本项目未选用 | 存在，未加入当前构建 |
| `examples/coverity/CMakeLists.txt` | 官方支持资料 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `examples/coverity/coverity_misra.config` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `examples/coverity/FreeRTOSConfig.h` | 【内核配置】示例 官方示例配置，非本项目配置 | 头文件：存在，当前无.d包含记录 |
| `examples/coverity/README.md` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `examples/README.md` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `examples/template_configuration/FreeRTOSConfig.h` | 【内核配置】示例 官方示例配置，非本项目配置 | 头文件：存在，当前无.d包含记录 |
| `examples/template_configuration/readme.md` | 官方支持资料 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |

</details>

<details>
<summary>include（23个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `include/atomic.h` | 【官方内核】 原子操作辅助 | 头文件：存在，当前无.d包含记录 |
| `include/CMakeLists.txt` | 官方支持资料 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `include/croutine.h` | 【官方内核】 官方源码包的构建/元数据/开发辅助文件 | 头文件：存在，当前无.d包含记录 |
| `include/deprecated_definitions.h` | 【官方内核】 旧类型/名称兼容 | 头文件：已有.d包含记录 |
| `include/event_groups.h` | 【官方内核】 事件组API | 头文件：存在，当前无.d包含记录 |
| `include/FreeRTOS.h` | 【官方内核】 公共入口、应用配置及默认值 | 头文件：已有.d包含记录 |
| `include/list.h` | 【官方内核】 内核链表定义 | 头文件：已有.d包含记录 |
| `include/message_buffer.h` | 【官方内核】 消息缓冲API宏 | 头文件：存在，当前无.d包含记录 |
| `include/mpu_prototypes.h` | 【官方内核】 MPU包装API原型 | 头文件：存在，当前无.d包含记录 |
| `include/mpu_syscall_numbers.h` | 【官方内核】 MPU系统调用编号 | 头文件：存在，当前无.d包含记录 |
| `include/mpu_wrappers.h` | 【官方内核】 MPU包装选择，当前非MPU端口也会包含 | 头文件：已有.d包含记录 |
| `include/newlib-freertos.h` | 【官方内核】 Newlib每任务C库支持 | 头文件：存在，当前无.d包含记录 |
| `include/picolibc-freertos.h` | 【官方内核】 Picolibc每任务C库支持 | 头文件：存在，当前无.d包含记录 |
| `include/portable.h` | 【移植层】 移植接口与堆API | 头文件：已有.d包含记录 |
| `include/projdefs.h` | 【官方内核】 基础类型/常量/毫秒换Tick | 头文件：已有.d包含记录 |
| `include/queue.h` | 【官方内核】 队列API及宏 | 头文件：已有.d包含记录 |
| `include/semphr.h` | 【官方内核】 信号量/互斥量API宏 | 头文件：存在，当前无.d包含记录 |
| `include/stack_macros.h` | 【官方内核】 栈溢出检查宏 | 头文件：已有.d包含记录 |
| `include/StackMacros.h` | 【官方内核】 旧拼写兼容入口，包含stack_macros.h并给出迁移警告 | 头文件：存在，当前无.d包含记录 |
| `include/stdint.readme` | 官方支持资料 整数类型说明 | 存在，不参与目标编译 |
| `include/stream_buffer.h` | 【官方内核】 流缓冲API | 头文件：存在，当前无.d包含记录 |
| `include/task.h` | 【官方内核】 任务/通知API与版本号 | 头文件：已有.d包含记录 |
| `include/timers.h` | 【官方内核】 软件定时器API与命令宏 | 头文件：已有.d包含记录 |

</details>

<details>
<summary>portable/ARMClang（1个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/ARMClang/Use-the-GCC-ports.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |

</details>

<details>
<summary>portable/ARMv8M（52个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/ARMv8M/copy_files.py` | 【移植层】 移植包文件组织/构建辅助脚本，不在MCU运行 | 存在，不参与目标编译 |
| `portable/ARMv8M/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM23_NTZ/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM23_NTZ/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM23_NTZ/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM23/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM23/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM23/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM33_NTZ/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM33_NTZ/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM33_NTZ/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM33/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM33/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM33/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM35P/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM52/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM55/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_CM85/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/GCC/ARM_STAR_MC3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM23_NTZ/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM23_NTZ/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM23_NTZ/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM23/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM23/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM23/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM33_NTZ/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM33_NTZ/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM33_NTZ/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM33/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM33/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM33/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM35P/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM52/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM55/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_CM85/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portable/IAR/ARM_STAR_MC3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/non_secure/ReadMe.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ARMv8M/ReadMe.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ARMv8M/secure/context/portable/GCC/ARM_CM23/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/secure/context/portable/GCC/ARM_CM33/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/secure/context/portable/IAR/ARM_CM23/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/secure/context/portable/IAR/ARM_CM33/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/secure/context/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/secure/context/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/secure/heap/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/secure/heap/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/secure/init/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/ARMv8M/secure/init/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/secure/macros/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/ARMv8M/secure/ReadMe.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |

</details>

<details>
<summary>portable/BCC（6个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/BCC/16BitDOS/common/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/BCC/16BitDOS/common/portcomn.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/BCC/16BitDOS/Flsh186/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/BCC/16BitDOS/Flsh186/prtmacro.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/BCC/16BitDOS/PC/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/BCC/16BitDOS/PC/prtmacro.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/CCRH（4个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/CCRH/F1Kx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/CCRH/F1Kx/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/CCRH/F1Kx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/CCRH/F1Kx/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |

</details>

<details>
<summary>portable/CCS（13个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/CCS/ARM_CM3/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/CCS/ARM_CM3/portasm.asm` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/CCS/ARM_CM3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/CCS/ARM_CM4F/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/CCS/ARM_CM4F/portasm.asm` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/CCS/ARM_CM4F/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/CCS/ARM_Cortex-R4/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/CCS/ARM_Cortex-R4/portASM.asm` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/CCS/ARM_Cortex-R4/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/CCS/MSP430X/data_model.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/CCS/MSP430X/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/CCS/MSP430X/portext.asm` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/CCS/MSP430X/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/CMakeLists.txt（1个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/CMakeLists.txt` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |

</details>

<details>
<summary>portable/CodeWarrior（8个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/CodeWarrior/ColdFire_V1/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/CodeWarrior/ColdFire_V1/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/CodeWarrior/ColdFire_V1/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/CodeWarrior/ColdFire_V2/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/CodeWarrior/ColdFire_V2/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/CodeWarrior/ColdFire_V2/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/CodeWarrior/HCS12/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/CodeWarrior/HCS12/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/Common（2个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/Common/mpu_wrappers_v2.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/Common/mpu_wrappers.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |

</details>

<details>
<summary>portable/GCC（284个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/GCC/ARM_AARCH64_SRE/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_AARCH64_SRE/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_AARCH64_SRE/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_AARCH64_SRE/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/ARM_AARCH64/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_AARCH64/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_AARCH64/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_AARCH64/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/ARM_CA53_64_BIT_SRE/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/ARM_CA53_64_BIT/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/ARM_CA9/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CA9/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CA9/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM0/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM0/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM0/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM0/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM0/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23_NTZ/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23_NTZ/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23/secure/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM23/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM23/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM3_MPU/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM3_MPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM3_MPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM3/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33/secure/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM33/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM33/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P_NTZ/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P_NTZ/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P/secure/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM35P/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM35P/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM4_MPU/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM4_MPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM4_MPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM4F/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM4F/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52_NTZ/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52_NTZ/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52/secure/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM52/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM52/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55_NTZ/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55_NTZ/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55/secure/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM55/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM55/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM7/r0p1/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM7/r0p1/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM7/ReadMe.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/ARM_CM85_NTZ/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85_NTZ/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85/secure/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CM85/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CM85/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CR5/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CR5/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CR5/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CR82/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CR82/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CR82/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CR82/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CR82/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/ARM_CRx_MPU/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CRx_MPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CRx_MPU/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CRx_MPU/portmacro_asm.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CRx_MPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_CRx_No_GIC/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CRx_No_GIC/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_CRx_No_GIC/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3_NTZ/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3_NTZ/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3/secure/secure_context_port.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM_STAR_MC3/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM_STAR_MC3/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM7_AT91FR40008/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_AT91FR40008/portISR.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_AT91FR40008/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM7_AT91SAM7S/AT91SAM7X256.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM7_AT91SAM7S/ioat91sam7x256.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM7_AT91SAM7S/lib_AT91SAM7X256.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_AT91SAM7S/lib_AT91SAM7X256.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM7_AT91SAM7S/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_AT91SAM7S/portISR.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_AT91SAM7S/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM7_LPC2000/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_LPC2000/portISR.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_LPC2000/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ARM7_LPC23xx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_LPC23xx/portISR.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ARM7_LPC23xx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ATMega323/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ATMega323/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/AVR_AVRDx/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/AVR_Mega0/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/AVR32_UC3/exception.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/GCC/AVR32_UC3/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/AVR32_UC3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/ColdFire_V2/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/ColdFire_V2/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/ColdFire_V2/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/CORTUS_APS3/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/CORTUS_APS3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/H8S2329/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/H8S2329/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/HCS12/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/HCS12/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/IA32_flat/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/IA32_flat/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/IA32_flat/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/IA32_flat/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/MCF5235/readme.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/MicroBlaze/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/MicroBlaze/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/MicroBlaze/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/MicroBlazeV8/port_exceptions.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/GCC/MicroBlazeV8/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/MicroBlazeV8/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/MicroBlazeV8/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/MicroBlazeV9/port_exceptions.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/GCC/MicroBlazeV9/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/MicroBlazeV9/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/MicroBlazeV9/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/MSP430F449/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/MSP430F449/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/NiosII/port_asm.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/GCC/NiosII/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/NiosII/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/PPC405_Xilinx/FPU_Macros.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/PPC405_Xilinx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/PPC405_Xilinx/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/PPC405_Xilinx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/PPC440_Xilinx/FPU_Macros.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/PPC440_Xilinx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/PPC440_Xilinx/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/PPC440_Xilinx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RISC-V/chip_extensions.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/GCC/RISC-V/chip_specific_extensions/Pulpino_Vega_RV32M1RM/freertos_risc_v_chip_specific_extensions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RISC-V/chip_specific_extensions/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/RISC-V/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions/freertos_risc_v_chip_specific_extensions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RISC-V/chip_specific_extensions/RISCV_no_extensions/freertos_risc_v_chip_specific_extensions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RISC-V/chip_specific_extensions/RV32I_CLINT_no_extensions/freertos_risc_v_chip_specific_extensions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RISC-V/Documentation.url` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/RISC-V/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/RISC-V/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/RISC-V/portContext.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RISC-V/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RISC-V/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/RL78/isr_support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RL78/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/RL78/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/RL78/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RX100/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/RX100/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RX100/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/RX200/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/RX200/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RX600/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/RX600/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RX600/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/RX600v2/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/RX600v2/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RX600v2/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/RX700v3_DPFPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/RX700v3_DPFPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/RX700v3_DPFPU/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/GCC/STR75x/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/STR75x/portISR.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/GCC/STR75x/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/TriCore_1782/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/GCC/TriCore_1782/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/GCC/TriCore_1782/porttrap.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |

</details>

<details>
<summary>portable/IAR（262个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/IAR/78K0R/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/78K0R/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/78K0R/portasm.s26` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/78K0R/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CA5_No_GIC/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CA5_No_GIC/portASM.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CA5_No_GIC/portASM.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CA5_No_GIC/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CA9/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CA9/portASM.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CA9/portASM.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CA9/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM0/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM0/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM0/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23_NTZ/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23_NTZ/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23/secure/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM23/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM23/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM3/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM3/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33_NTZ/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33_NTZ/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33/secure/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM33/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM33/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P_NTZ/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P_NTZ/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P/secure/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM35P/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM35P/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM4F_MPU/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM4F_MPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM4F_MPU/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM4F_MPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM4F/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM4F/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM4F/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52_NTZ/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52_NTZ/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52/secure/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM52/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM52/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55_NTZ/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55_NTZ/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55/secure/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM55/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM55/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM7/r0p1/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM7/r0p1/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM7/r0p1/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM7/ReadMe.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/IAR/ARM_CM85_NTZ/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85_NTZ/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85/secure/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CM85/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CM85/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_CRx_No_GIC/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CRx_No_GIC/portASM.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_CRx_No_GIC/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3_NTZ/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3_NTZ/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3/non_secure/mpu_wrappers_v2_asm.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3/non_secure/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3/secure/secure_context_port_asm.s` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3/secure/secure_context.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3/secure/secure_context.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3/secure/secure_heap.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3/secure/secure_heap.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3/secure/secure_init.c` | 【移植层】 该平台的安全/非安全上下文移植支持 | 存在，未加入当前构建 |
| `portable/IAR/ARM_STAR_MC3/secure/secure_init.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ARM_STAR_MC3/secure/secure_port_macros.h` | 【移植层】 该平台的安全/非安全上下文移植支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ATMega323/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/ATMega323/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/ATMega323/portmacro.s90` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/IAR/AtmelSAM7S64/AT91SAM7S64_inc.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/AT91SAM7S64.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/AT91SAM7X128_inc.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/AT91SAM7X128.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/AT91SAM7X256_inc.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/AT91SAM7X256.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/lib_AT91SAM7S64.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/lib_AT91SAM7X128.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/lib_AT91SAM7X256.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM7S64/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/AtmelSAM7S64/portasm.s79` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/AtmelSAM7S64/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM9XE/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AtmelSAM9XE/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/AtmelSAM9XE/portasm.s79` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/AtmelSAM9XE/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AVR_AVRDx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/AVR_AVRDx/porthardware.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AVR_AVRDx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AVR_AVRDx/portmacro.s90` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/IAR/AVR_Mega0/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/AVR_Mega0/porthardware.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AVR_Mega0/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AVR_Mega0/portmacro.s90` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/IAR/AVR32_UC3/exception.s82` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/IAR/AVR32_UC3/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/AVR32_UC3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/AVR32_UC3/read.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/IAR/AVR32_UC3/write.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/IAR/LPC2000/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/LPC2000/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/LPC2000/portasm.s79` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/LPC2000/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/MSP430/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/MSP430/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/MSP430/portext.s43` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/IAR/MSP430/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/MSP430X/data_model.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/MSP430X/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/MSP430X/portext.s43` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/IAR/MSP430X/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RISC-V/chip_extensions.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/IAR/RISC-V/chip_specific_extensions/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/IAR/RISC-V/chip_specific_extensions/RV32I_CLINT_no_extensions/freertos_risc_v_chip_specific_extensions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RISC-V/Documentation.url` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/IAR/RISC-V/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/RISC-V/portASM.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/RISC-V/portContext.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RISC-V/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RISC-V/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/IAR/RL78/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/RL78/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/IAR/RL78/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RX100/port_asm.s` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/IAR/RX100/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/RX100/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RX100/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/IAR/RX600/port_asm.s` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/IAR/RX600/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/RX600/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RX600/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/IAR/RX700v3_DPFPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/RX700v3_DPFPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RX700v3_DPFPU/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/IAR/RXv2/port_asm.s` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/IAR/RXv2/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/RXv2/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/RXv2/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/IAR/STR71x/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/STR71x/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/STR71x/portasm.s79` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/STR71x/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/STR75x/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/STR75x/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/STR75x/portasm.s79` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/STR75x/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/STR91x/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/STR91x/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/STR91x/portasm.s79` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/STR91x/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/V850ES/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/IAR/V850ES/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/IAR/V850ES/portasm_Fx3.s85` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/V850ES/portasm_Hx2.s85` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/V850ES/portasm.s85` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/IAR/V850ES/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/Keil（1个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/Keil/See-also-the-RVDS-directory.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |

</details>

<details>
<summary>portable/MemMang（6个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/MemMang/heap_1.c` | 【内存管理】 只分配不释放的官方堆策略 | 存在，未加入当前构建 |
| `portable/MemMang/heap_2.c` | 【内存管理】 可释放、不合并空闲块的官方堆策略 | 存在，未加入当前构建 |
| `portable/MemMang/heap_3.c` | 【内存管理】 C库malloc/free包装策略 | 存在，未加入当前构建 |
| `portable/MemMang/heap_4.c` | 【内存管理】 当前堆，分配/释放并合并相邻空闲块 | 已编译；有必要部分保留 |
| `portable/MemMang/heap_5.c` | 【内存管理】 多非连续区域堆策略 | 存在，未加入当前构建 |
| `portable/MemMang/ReadMe.url` | 【内存管理】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |

</details>

<details>
<summary>portable/MikroC（2个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/MikroC/ARM_CM4F/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/MikroC/ARM_CM4F/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/MPLAB（19个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/MPLAB/PIC18F/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC18F/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/MPLAB/PIC18F/stdio.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/MPLAB/PIC24_dsPIC/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC24_dsPIC/portasm_dsPIC.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC24_dsPIC/portasm_PIC24.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC24_dsPIC/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/MPLAB/PIC32MEC14xx/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/MPLAB/PIC32MEC14xx/port_asm.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC32MEC14xx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC32MEC14xx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/MPLAB/PIC32MX/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/MPLAB/PIC32MX/port_asm.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC32MX/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC32MX/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/MPLAB/PIC32MZ/ISR_Support.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/MPLAB/PIC32MZ/port_asm.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC32MZ/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/MPLAB/PIC32MZ/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/MSVC-MingW（2个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/MSVC-MingW/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/MSVC-MingW/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/oWatcom（6个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/oWatcom/16BitDOS/common/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/oWatcom/16BitDOS/common/portcomn.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/oWatcom/16BitDOS/Flsh186/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/oWatcom/16BitDOS/Flsh186/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/oWatcom/16BitDOS/PC/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/oWatcom/16BitDOS/PC/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/Paradigm（6个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/Paradigm/Tern_EE/large_untested/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Paradigm/Tern_EE/large_untested/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/Paradigm/Tern_EE/large_untested/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/Paradigm/Tern_EE/small/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Paradigm/Tern_EE/small/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/Paradigm/Tern_EE/small/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/readme.txt（1个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |

</details>

<details>
<summary>portable/Renesas（24个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/Renesas/RX100/port_asm.src` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，不参与目标编译 |
| `portable/Renesas/RX100/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Renesas/RX100/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/Renesas/RX100/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/Renesas/RX200/port_asm.src` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，不参与目标编译 |
| `portable/Renesas/RX200/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Renesas/RX200/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/Renesas/RX200/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/Renesas/RX600/port_asm.src` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，不参与目标编译 |
| `portable/Renesas/RX600/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Renesas/RX600/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/Renesas/RX600/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/Renesas/RX600v2/port_asm.src` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，不参与目标编译 |
| `portable/Renesas/RX600v2/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Renesas/RX600v2/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/Renesas/RX600v2/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/Renesas/RX700v3_DPFPU/port_asm.src` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，不参与目标编译 |
| `portable/Renesas/RX700v3_DPFPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Renesas/RX700v3_DPFPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/Renesas/RX700v3_DPFPU/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/Renesas/SH2A_FPU/ISR_Support.inc` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/Renesas/SH2A_FPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Renesas/SH2A_FPU/portasm.src` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，不参与目标编译 |
| `portable/Renesas/SH2A_FPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/Rowley（5个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/Rowley/ARM7/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/Rowley/MSP430F449/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Rowley/MSP430F449/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/Rowley/MSP430F449/portext.asm` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/Rowley/MSP430F449/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/RVDS（20个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/RVDS/ARM_CA9/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/RVDS/ARM_CA9/portASM.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/RVDS/ARM_CA9/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/RVDS/ARM_CA9/portmacro.inc` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，不参与目标编译 |
| `portable/RVDS/ARM_CM0/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/RVDS/ARM_CM0/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/RVDS/ARM_CM3/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 已编译；有必要部分保留 |
| `portable/RVDS/ARM_CM3/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：已有.d包含记录 |
| `portable/RVDS/ARM_CM4_MPU/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/RVDS/ARM_CM4_MPU/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/RVDS/ARM_CM4_MPU/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/RVDS/ARM_CM4F/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/RVDS/ARM_CM4F/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/RVDS/ARM_CM7/r0p1/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/RVDS/ARM_CM7/r0p1/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/RVDS/ARM_CM7/ReadMe.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/RVDS/ARM7_LPC21xx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/RVDS/ARM7_LPC21xx/portASM.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/RVDS/ARM7_LPC21xx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/RVDS/ARM7_LPC21xx/portmacro.inc` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，不参与目标编译 |

</details>

<details>
<summary>portable/SDCC（2个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/SDCC/Cygnal/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/SDCC/Cygnal/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/Softune（6个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/Softune/MB91460/__STD_LIB_sbrk.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/Softune/MB91460/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Softune/MB91460/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/Softune/MB96340/__STD_LIB_sbrk.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/Softune/MB96340/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Softune/MB96340/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/Tasking（3个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/Tasking/ARM_CM4F/port_asm.asm` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/Tasking/ARM_CM4F/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/Tasking/ARM_CM4F/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/template（2个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/template/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/template/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/ThirdParty（169个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/ThirdParty/CDK/T-HEAD_CK802/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/CDK/T-HEAD_CK802/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/CDK/T-HEAD_CK802/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/CCRH/RH850_F1KM_S4/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/CCRH/RH850_F1KM_S4/portasm.s` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/CCRH/RH850_F1KM_S4/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/CCRH/RH850_F1KM_S4/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/CCS/C2000_C28x/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/CCS/C2000_C28x/portasm.asm` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/CCS/C2000_C28x/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/CCS/C2000_C28x/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/bsp_patches/boot.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/bsp_patches/cpu.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/bsp_patches/cpu.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/bsp_patches/ReadMe.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/bsp_patches/scugic_v4_2_diff.png` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/bsp_patches/standalone_v7_2_diff.png` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/bsp_patches/xil-crt0.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/bsp_patches/xscugic.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/port_asm_vectors.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/portZynqUltrascale.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/CORTEX_A53_64-bit_UltraScale_MPSoC/ReadMe.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/MSP430FR5969/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/MSP430FR5969/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RISC-V/chip_specific_extensions/THEAD_RV32/freertos_risc_v_chip_specific_extensions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/CMakeLists.txt` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/library.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/LICENSE.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure/freertos_sdk_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure/mpu_wrappers_v2_asm.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure/portasm.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure/portasm.h` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure/portmacrocommon.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure/rp2040_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/pico_sdk_import.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/CMakeLists.txt` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/Documentation.url` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/FreeRTOS_Kernel_import.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/include/freertos_risc_v_chip_specific_extensions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/include/freertos_sdk_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/include/portContext.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/include/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/include/rp2040_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/library.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/LICENSE.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/notes.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/pico_sdk_import.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/TriCore_38xa/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/TriCore_38xa/port.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/TriCore_38xa/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/TriCore_38xa/porttrap.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/GCC/TriCore_38xa/readme.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/LICENSE` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Community-Supported-Ports/Z88DK/Z180/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Community-Supported-Ports/Z88DK/Z180/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Community-Supported-Ports/Z88DK/Z180/readme.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/ARC_EM_HS/arc_freertos_exceptions.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ARC_EM_HS/arc_freertos_exceptions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/ARC_EM_HS/arc_support.s` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ARC_EM_HS/freertos_tls.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ARC_EM_HS/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ARC_EM_HS/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/ARC_v1/arc_freertos_exceptions.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ARC_v1/arc_freertos_exceptions.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/ARC_v1/arc_support.s` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ARC_v1/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ARC_v1/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/ARM_TFM/os_wrapper_freertos.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ARM_TFM/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/ATmega/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/ATmega/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/ATmega/readme.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/Posix/FreeRTOS-simulator-for-Linux.url` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/Posix/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Posix/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Posix/utils/wait_for_event.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Posix/utils/wait_for_event.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/RISC-V/README-for-info-on-official-MIT-license-port.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/RP2040/CMakeLists.txt` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/RP2040/include/freertos_sdk_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/RP2040/include/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/RP2040/include/rp2040_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/RP2040/library.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/RP2040/LICENSE.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/RP2040/pico_sdk_import.cmake` | 【移植层】 CMake构建组织，当前Keil/EIDE不使用 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/RP2040/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/RP2040/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/FreeRTOS-openocd.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/FreeRTOSConfig_arch.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/port_systick.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/portbenchmark.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/xt_asm_utils.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/xtensa_api.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/xtensa_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/xtensa_context.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/xtensa_rtos.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/include/xtensa_timer.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/port_common.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/port_systick.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/portmux_impl.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/portmux_impl.inc.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/xtensa_context.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/xtensa_init.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/xtensa_loadstore_handler.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/xtensa_overlay_os_hook.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/xtensa_vector_defaults.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/GCC/Xtensa_ESP32/xtensa_vectors.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/KnownIssues.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/asm-offsets.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/Makefile` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/mpu.S` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/portbenchmark.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/portclib.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/portmpu.c` | 【移植层】 该平台的MPU/系统调用包装支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/porttrace.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/readme_xtensa.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/relnotes.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_api.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_context.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_context.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_coproc_handler.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_intr_asm.S` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_intr_wrapper.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_intr.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_overlay_os_hook.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_rtos.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_timer.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_vectors_xea3.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Cadence/Xtensa/xtensa_vectors.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/GCC/AVR_AVRDx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/GCC/AVR_AVRDx/porthardware.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/GCC/AVR_AVRDx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/GCC/AVR_Mega0/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/GCC/AVR_Mega0/porthardware.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/GCC/AVR_Mega0/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/LICENSE` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Partner-Supported-Ports/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/Partner-Supported-Ports/Tasking/AURIX_TC3xx/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/Tasking/AURIX_TC3xx/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/Partner-Supported-Ports/TI/CORTEX_A53_64-BIT_TI_AM64_SMP/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/TI/CORTEX_A53_64-BIT_TI_AM64_SMP/portASM.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/Partner-Supported-Ports/TI/CORTEX_A53_64-BIT_TI_AM64_SMP/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/README.md` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/XCC/Xtensa/readme_xtensa.txt` | 【移植层】 该目录的许可/说明/历史/文档资料，不是目标源码 | 存在，不参与目标编译 |
| `portable/ThirdParty/xClang/XCOREAI/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/ThirdParty/xClang/XCOREAI/port.xc` | 【移植层】 该目录对应平台的移植/构建配套文件 | 存在，不参与目标编译 |
| `portable/ThirdParty/xClang/XCOREAI/portasm.S` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/ThirdParty/xClang/XCOREAI/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |
| `portable/ThirdParty/xClang/XCOREAI/rtos_support_rtos_config.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |

</details>

<details>
<summary>portable/WizC（6个文件）</summary>

| 实际相对路径 | 类型与作用 | 当前状态 |
| --- | --- | --- |
| `portable/WizC/PIC18/addFreeRTOS.h` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 头文件：存在，当前无.d包含记录 |
| `portable/WizC/PIC18/Drivers/Tick/isrTick.c` | 【移植层】 该平台的中断入口/上下文切换汇编支持 | 存在，未加入当前构建 |
| `portable/WizC/PIC18/Drivers/Tick/Tick.c` | 【移植层】 该目录对应平台的移植源码/接口/汇编辅助 | 存在，未加入当前构建 |
| `portable/WizC/PIC18/Install.bat` | 【移植层】 移植包文件组织/构建辅助脚本，不在MCU运行 | 存在，不参与目标编译 |
| `portable/WizC/PIC18/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 存在，未加入当前构建 |
| `portable/WizC/PIC18/portmacro.h` | 【移植层】 该架构/编译器的端口类型、临界区和宏 | 头文件：存在，当前无.d包含记录 |

</details>


## 附录 B. 当前构建实际选中的48个源文件

本表直接取现存 `builder.params.sourceList`，并与Keil/EIDE清单交叉核对。路径以MDK-ARM为基准。**所有行都参与编译**；最后一列额外区分是否有内容保留到现存最终map。源码编译成功后也可能被整个裁剪。

| 实际路径 | 作用 | 现存最终链接状态 |
| --- | --- | --- |
| `../Core/Src/main.c` | 【用户应用】【STM32驱动】主入口、GPIO/I2C初始化、现有GUI_Task | 至少有部分代码/数据保留 |
| `../Core/Src/stm32f1xx_hal_msp.c` | 【STM32驱动】HAL底层初始化和优先级分组 | 至少有部分代码/数据保留 |
| `../Core/Src/stm32f1xx_it.c` | 【STM32驱动】中断/异常分派 | 至少有部分代码/数据保留 |
| `../Core/Src/system_stm32f1xx.c` | 【STM32驱动】系统初始化和SystemCoreClock | 至少有部分代码/数据保留 |
| `../Drivers/DShanMCU-F103/driver_ir_receiver.c` | 【STM32驱动】红外采样、解析、缓存和读取 | 至少有部分代码/数据保留 |
| `../Drivers/DShanMCU-F103/driver_irq.c` | 【STM32驱动】EXTI回调转交红外 | 至少有部分代码/数据保留 |
| `../Drivers/DShanMCU-F103/driver_lcd.c` | 【STM32驱动】LCD到OLED的显示适配 | 至少有部分代码/数据保留 |
| `../Drivers/DShanMCU-F103/driver_oled.c` | 【STM32驱动】OLED发送及帧缓冲 | 至少有部分代码/数据保留 |
| `../Drivers/DShanMCU-F103/driver_systick.c` | 【STM32驱动】实际TIM4 HAL毫秒时基 | 至少有部分代码/数据保留 |
| `../Drivers/DShanMCU-F103/driver_timer.c` | 【STM32驱动】TIM3微秒计时 | 至少有部分代码/数据保留 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c` | 【STM32驱动】HAL初始化/Tick | 至少有部分代码/数据保留 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c` | 【STM32驱动】NVIC与Cortex接口 | 至少有部分代码/数据保留 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c` | 【STM32驱动】DMA底层 | 当前未用部分全部裁剪 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c` | 【STM32驱动】EXTI配置接口 | 当前未用部分全部裁剪 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c` | 【STM32驱动】Flash接口 | 当前未用部分全部裁剪 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c` | 【STM32驱动】Flash扩展 | 当前未用部分全部裁剪 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c` | 【STM32驱动】GPIO与EXTI回调分派 | 至少有部分代码/数据保留 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c` | 【STM32驱动】GPIO扩展 | 当前未用部分全部裁剪 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c` | 【STM32驱动】OLED使用的I2C接口 | 至少有部分代码/数据保留 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c` | 【STM32驱动】电源接口 | 当前未用部分全部裁剪 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c` | 【STM32驱动】系统时钟配置 | 至少有部分代码/数据保留 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c` | 【STM32驱动】时钟扩展 | 当前未用部分全部裁剪 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c` | 【STM32驱动】TIM基础接口 | 至少有部分代码/数据保留 |
| `../Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c` | 【STM32驱动】TIM扩展 | 当前未用部分全部裁剪 |
| `../NWatch/animation.c` | 【NWatch GUI】动画状态机 | 至少有部分代码/数据保留 |
| `../NWatch/appconfig.c` | 【NWatch GUI】应用配置状态 | 至少有部分代码/数据保留 |
| `../NWatch/buttons.c` | 【NWatch GUI】按钮轮询及动作回调 | 至少有部分代码/数据保留 |
| `../NWatch/display.c` | 【NWatch GUI】刷新与显示状态机 | 至少有部分代码/数据保留 |
| `../NWatch/draw.c` | 【NWatch GUI】绘图接口 | 至少有部分代码/数据保留 |
| `../NWatch/global.c` | 【NWatch GUI】全局更新 | 至少有部分代码/数据保留 |
| `../NWatch/m_display.c` | 【NWatch GUI】显示菜单 | 至少有部分代码/数据保留 |
| `../NWatch/m_main.c` | 【NWatch GUI】主菜单 | 至少有部分代码/数据保留 |
| `../NWatch/menu.c` | 【NWatch GUI】菜单机制 | 至少有部分代码/数据保留 |
| `../NWatch/millis.c` | 【NWatch GUI】读取HAL/TIM4毫秒时间 | 至少有部分代码/数据保留 |
| `../NWatch/normal.c` | 【NWatch GUI】普通表盘 | 至少有部分代码/数据保留 |
| `../NWatch/pwrmgr.c` | 【NWatch GUI】活动/电源管理 | 至少有部分代码/数据保留 |
| `../NWatch/resources.c` | 【NWatch GUI】只读图形资源 | 至少有部分代码/数据保留 |
| `../NWatch/time.c` | 【NWatch GUI】时间更新 | 至少有部分代码/数据保留 |
| `../NWatch/tunemaker.c` | 【NWatch GUI】当前立方体等演示 | 至少有部分代码/数据保留 |
| `../NWatch/ui.c` | 【NWatch GUI】UI接口 | 至少有部分代码/数据保留 |
| `../NWatch/watchface.c` | 【NWatch GUI】表盘与绘图数据 | 至少有部分代码/数据保留 |
| `RTOS/rtos_app.c` | 【用户应用】任务创建、诊断、钩子与故障捕获 | 至少有部分代码/数据保留 |
| `ThirdParty/FreeRTOS-Kernel/list.c` | 【官方内核】 内核双向链表实现 | 至少有部分代码/数据保留 |
| `ThirdParty/FreeRTOS-Kernel/portable/MemMang/heap_4.c` | 【内存管理】 当前堆，分配/释放并合并相邻空闲块 | 至少有部分代码/数据保留 |
| `ThirdParty/FreeRTOS-Kernel/portable/RVDS/ARM_CM3/port.c` | 【移植层】 该架构/编译器的内核移植实现 | 至少有部分代码/数据保留 |
| `ThirdParty/FreeRTOS-Kernel/queue.c` | 【官方内核】 队列/信号量/互斥量基础实现 | 当前未用部分全部裁剪 |
| `ThirdParty/FreeRTOS-Kernel/tasks.c` | 【官方内核】 任务调度/创建/延时/通知（各API受配置裁剪） | 至少有部分代码/数据保留 |
| `startup_stm32f103xb.s` | 【STM32驱动】向量表、复位、MSP与C库堆 | 至少有部分代码/数据保留 |


## 附录 C. RTOS来源、验收与历史产物目录扫描

这些路径均实际存在，**都不是现行sourceList的固件源码**。备份中的同名.c/.h也不参与现行编译。下表补充列出FreeRTOS来源、验收与早期内核验证资料；不把调试日志和压缩包误计为内核模块。

<details>
<summary>展开支持资料逐文件列表（47个）</summary>

| 实际路径（MDK-ARM为基准） | 作用/状态 |
| --- | --- |
| `acceptance/REPORT.md` | 已有分析/验收报告，不编译 |
| `acceptance/STATIC_REPORT.md` | 已有分析/验收报告，不编译 |
| `acceptance/before/NWatch-DShanMCU-F103.axf` | 历史源码/构建备份，不编译 |
| `acceptance/before/NWatch-DShanMCU-F103.hex` | 历史源码/构建备份，不编译 |
| `acceptance/before/NWatch-DShanMCU-F103.map` | 历史源码/构建备份，不编译 |
| `acceptance/before/rtos_app.c` | 历史源码/构建备份，不编译 |
| `acceptance/before/rtos_app.h` | 历史源码/构建备份，不编译 |
| `acceptance/board-flash-readback.bin` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/cube-cli-help.txt` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/disassembly.txt` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/final-verification.json` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/flash-read-comparison.json` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/flash-read-diagnostic.json` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/gdb-symbols.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/observe.gdb` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/read_runtime.py` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/rebuild.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/runtime-check.gdb` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/runtime-check.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/runtime-connect.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/runtime-observations.json` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/runtime-observations.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/source-changes.diff` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/source-hashes-before.json` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/startup-check.gdb` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/startup-check.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/stlink-probe.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/stlink-runtime-probe.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/verification-output.log` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/verification.json` | 历史验收/调试/日志/比对资料，不编译 |
| `acceptance/verify_image.py` | 历史验收/调试/日志/比对资料，不编译 |
| `phase3_freertos/FreeRTOS-KernelV11.3.1.zip` | 源码/工程压缩包留档，不编译 |
| `phase3_freertos/baseline-build.log` | 历史验收/调试/日志/比对资料，不编译 |
| `phase3_freertos/baseline/manifest.json` | 历史基线留档，不编译 |
| `phase3_freertos/baseline/timebase-project.zip` | 历史基线留档，不编译 |
| `phase3_freertos/disassembly.txt` | 历史验收/调试/日志/比对资料，不编译 |
| `phase3_freertos/gui-build.log` | 历史验收/调试/日志/比对资料，不编译 |
| `phase3_freertos/kernel-bringup-build.log` | 历史验收/调试/日志/比对资料，不编译 |
| `phase3_freertos/kernel-bringup/builder.params` | 早期内核验证构建产物，不是现行镜像 |
| `phase3_freertos/kernel-bringup/kernel-bringup.hex` | 早期内核验证构建产物，不是现行镜像 |
| `phase3_freertos/kernel-bringup/kernel-bringup.map` | 早期内核验证构建产物，不是现行镜像 |
| `phase3_freertos/source-origin.json` | 官方版本/下载与摘要来源记录，不编译 |
| `phase3_freertos/stlink-probe.log` | 历史验收/调试/日志/比对资料，不编译 |
| `scripts/configure_freertos.cjs` | 历史主机工具脚本，本轮未运行 |
| `scripts/fetch_freertos.py` | 历史主机工具脚本，本轮未运行 |
| `scripts/integrate_freertos.py` | 历史主机工具脚本，本轮未运行 |
| `scripts/verify_freertos.py` | 历史主机工具脚本，本轮未运行 |

</details>
