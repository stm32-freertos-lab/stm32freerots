# NWatch 多任务架构可行性评估

- 评估日期：2026-11-07
- 评估对象：`NWatch-DShanMCU-F103-RTOS` 当前 FreeRTOS 单 GUI 版本
- 评估性质：只读分析，未修改源码、配置、驱动、任务或工程结构
- 依据：当前 MAP/AXF/链接报告、`FreeRTOSConfig.h`、`main.c`、`RTOS/rtos_app.c`、`driver_dht11.c`、`driver_oled.c`、`driver_timer.c`、`display.c`、`buttons.c`、`docs/board_hardware.md`、EIDE 构建清单

> 说明：用户提供的 mark 架构文档以 `text/plain` 附件形式发送，当前会话未能读到正文；本评估依据用户文字描述的三任务方案与架构图，以及工程内实际源码。

---

## 1. 当前工程实际情况

### 1.1 芯片与容量

| 项目 | 实际值 | 证据 |
|---|---:|---|
| MCU | STM32F103C8T6，LQFP48 | `NWatch-DShanMCU-F103.ioc:10,21,47`；`docs/board_hardware.md:5-6` |
| Flash | 64 KB | 器件型号；MAP `LR_IROM1 Max 0x00010000` |
| SRAM | 20,480 B | MAP `RW_IRAM1 Max 0x00005000` |
| 当前 Flash 占用 | 35,676 B，余 29,860 B | MAP/验收记录 |
| 当前静态 RAM | 11,256 B（RW 356 + ZI 10,900） | MAP 尾部 `Total RW Size 11256` |
| 链接器未分配 RAM | 9,224 B | 20,480 − 11,256 |
| FreeRTOS Heap | 6,144 B，已含在 11,256 B 内 | `FreeRTOSConfig.h:25`；MAP `ucHeap 0x20000D0C` |
| Heap 当前空闲 | 2,384 B（历史最小同值） | `acceptance/runtime-observations.json` |
| TCB 大小 | 64 B | `acceptance/verification.json` `tcb_bytes=64` |

### 1.2 当前任务与调度

| 项目 | 实际值 | 证据 |
|---|---|---|
| 任务数 | 2：GUI_Task + Idle_Task | `rtos_app.c:51-52`；运行时 `taskCount=2` |
| GUI 优先级 | 2 | `rtos_app.h:11` |
| Idle 优先级 | 0 | FreeRTOS 默认 |
| GUI 栈 | 768 字 = 3,072 B | `rtos_app.h:10` |
| Idle 栈 | 128 字 = 512 B | `FreeRTOSConfig.h:22` |
| GUI 栈历史最低剩余 | 686 字 = 2,744 B（峰值 328 B） | `acceptance/REPORT.md`；`runtime-observations.json` |
| Idle 栈历史最低剩余 | 110 字 = 440 B（峰值 72 B） | 同上 |
| Tick | 1 kHz，SysTick 专供内核 | `FreeRTOSConfig.h:11,19` |
| 时基 | TIM4 = HAL 1 ms；TIM3 = 1 µs | `driver_systick.c`、`driver_timer.c` |
| 抢占/时间片 | 都开启 | `FreeRTOSConfig.h:13-14` |
| 堆管理 | heap_4 | `heap_4.c`；MAP `ucHeap 6144` |
| 栈溢出检测 | 模式 2 + 钩子 | `FreeRTOSConfig.h:28` |
| malloc 失败钩子 | 开启 | `FreeRTOSConfig.h:27` |

### 1.3 GUI_Task 的实际循环

`main.c:213-253`：

```c
for(;;)
{
    time_update();
    if(pwrmgr_userActive()) { buttons_update(); }
    global_update();
    if(pwrmgr_userActive()) {
        mMainUpdate();      // 仅 NWATCH_UI_ONLY=1
        display_update();   // 绘制 + 刷屏
    }
    pwrmgr_update();
    NWatch_RtosPoll();
    vTaskDelay(pdMS_TO_TICKS(1U));
}
```

- `display_update()` 内部按 60 FPS（忙碌）/25 FPS（空闲）限帧，见 `display.c:16-23,69-138`。
- `draw_end()` → `LCD_Flush()` → `OLED_Flush()`，见 `draw.c:172-176`、`driver_lcd.c:176-179`。
- `OLED_Flush()` 分 8 页、每页 128 B 调用 `HAL_I2C_Mem_Write(..., 500ms)`，见 `driver_oled.c:710-720,173-176`。
- I2C 400 kHz 下，128 B 一页约 3 ms，8 页约 **24 ms/帧** 的阻塞式发送；这是 GUI 任务的主要阻塞源。

### 1.4 红外按键

- `buttons.c:47-126` 每 10 ms 调一次 `IRReceiver_Read()`。
- IR 接收在 PB10 EXTI 中记录 TIM3 微秒时间戳，环形缓冲 128 B，见 `driver_ir_receiver.c:40-45,233-280`。
- 按键事件只在 GUI_Task 内被消费，不存在跨任务访问。
- IR ISR 优先级 5，高于 SysTick/PendSV（15），见 `FreeRTOSConfig.h:56-62`、`rtos_app.c:37-42`。

### 1.5 DHT11 现状（关键）

| 事实 | 证据 |
|---|---|
| DHT11 数据线在 **PA1** | `driver_dht11.c:81,97,114`；`docs/board_hardware.md:18,92` |
| PA1 在 `MX_GPIO_Init()` 中已配置为 **开漏输出 + 上拉**，并初始输出低 | `main.c:557-567` |
| DHT11 驱动 **未参与当前构建** | `NWatch-DShanMCU-F103.uvprojx:648-654` `IncludeInBuild=0`；`builder.params`/`compile_commands.json` 无 `driver_dht11.c`；MAP 无 `dht11.o` |
| `DHT11_Init()` 被 `#if !NWATCH_UI_ONLY` 排除 | `main.c:162-166` |
| `normal.c` 的温湿度显示也被 `#if !NWATCH_UI_ONLY` 排除 | `normal.c:130-147` |
| 当前构建 `NWATCH_UI_ONLY=1` | `NWatch/config.h:40-42` |
| 蜂鸣器/LED/温湿度驱动未编译 | `builder.params` 源文件列表；`main.c:162-166` |

### 1.6 当前构建实际包含的源文件

`compile_commands.json` 中与业务相关的只有：

- `Core/Src/main.c`、`stm32f1xx_hal_msp.c`、`stm32f1xx_it.c`、`system_stm32f1xx.c`
- `Drivers/DShanMCU-F103/`：`driver_ir_receiver.c`、`driver_irq.c`、`driver_lcd.c`、`driver_oled.c`、`driver_systick.c`、`driver_timer.c`
- `NWatch/`：`animation.c`、`appconfig.c`、`buttons.c`、`display.c`、`draw.c`、`global.c`、`m_display.c`、`m_main.c`、`menu.c`、`millis.c`、`normal.c`、`pwrmgr.c`、`resources.c`、`time.c`、`tunemaker.c`、`ui.c`、`watchface.c`
- FreeRTOS：`list.c`、`queue.c`、`tasks.c`、`heap_4.c`、`RVDS/ARM_CM3/port.c`

**没有** `driver_dht11.c`、`driver_buzzer.c`、`driver_color_led.c`。

### 1.7 当前 RTOS 能力边界

`FreeRTOSConfig.h`：

| 能力 | 配置 | 结论 |
|---|---|---|
| 动态分配 | `configSUPPORT_DYNAMIC_ALLOCATION=1` | 可动态创建任务/队列 |
| 静态分配 | `configSUPPORT_STATIC_ALLOCATION=0` | 不能用静态 TCB/队列 |
| Mutex | `configUSE_MUTEXES=0` | **不可用** |
| 递归 Mutex | `0` | 不可用 |
| 计数信号量 | `configUSE_COUNTING_SEMAPHORES=0` | **不可用** |
| 任务通知 | `configUSE_TASK_NOTIFICATIONS=0` | **不可用** |
| 软件定时器 | `configUSE_TIMERS=0` | **不可用**，无 Timer 任务 |
| 事件组 | `event_groups.c` 不在构建列表 | **不可用** |
| 队列 | `queue.c` 已编译 | **可用** |
| 二值信号量 | `semphr.h:167` 宏映射到 `xQueueGenericCreate` | 可用，但队列更合适 |
| 任务删除/挂起/改优先级 | `INCLUDE_vTaskDelete/Suspend/PrioritySet=0` | **不可用** |
| 栈水位 API | `INCLUDE_uxTaskGetStackHighWaterMark=1` | 可用 |

---

## 2. 总体方案是否可行

### 结论：**有条件可行**

- **Sensor_Task 独立创建：可行且合理**，因为 DHT11 是位时序协议，必须避免在逐位采样期间被 GUI 抢占。
- **Pomodoro_Task 独立创建：可行但没有必要**。它只做计数和状态机，不阻塞、不访问硬件；独立任务会多花约 592 B 堆、一个 TCB 和一个事件队列，收益接近于零。
- **GUI_Task 不应当拥有最高应用优先级**。若 Sensor 独立且 GUI 优先级更高，DHT11 的逐位读取会被 GUI 每 1 ms 抢占一次，读取必失败。要么 Sensor 优先级 3 > GUI 2，要么把 DHT11 放进 GUI 任务。

### 必须满足的条件

1. 确认实板安装的是 **M10 DHT11**，不是 M2 DS18B20；两者共用 PA1，原理图注明不能同时接（`docs/board_hardware.md:18,151`）。
2. 确认 DHT11 模块自带数据线上拉，或补 4.7–10 kΩ 上拉；PA1 内部上拉偏弱。
3. 接受 **Sensor 优先级 3、GUI 优先级 2**，即 GUI 不是最高应用优先级。
4. 番茄钟 **合并进 GUI**（推荐）；若坚持独立任务，需接受 Heap 只剩约 768 B。
5. 任务间通信只用 **队列** 或临界区；不要使用当前未启用的 mutex、事件组、任务通知。
6. DHT11 驱动需 **单独加入 EIDE 构建**；不要为了 DHT11 直接把 `NWATCH_UI_ONLY` 改为 0，否则会连带引入蜂鸣器、LED、LCD 控制台等，Flash/RAM 和初始化路径都会扩大。
7. 番茄钟结束提示若需要声音，必须另加蜂鸣器驱动；PA8 与 M4 红外发射管互斥（`docs/board_hardware.md:25,152`），当前 UI-only 构建没有蜂鸣器驱动。

---

## 3. Flash 和 SRAM 资源分析

### 3.1 当前总账（不重复计算）

```text
SRAM 20,480 B
= 静态 RAM 11,256 B
  = FreeRTOS Heap 6,144 B（容器，含 GUI/Idle 栈与 TCB）
  + 主栈 MSP 1,024 B
  + OLED 帧缓冲 1,024 B
  + Cube 画布 ×2 1,024 B
  + C 库堆 512 B
  + 红外驱动 424 B
  + NWatch UI 状态 332 B
  + HAL/定时器 252 B
  + FreeRTOS 内核静态 248 B
  + 诊断结构 164 B
  + libspace 96 B
  + 填充 12 B
+ 未分配 9,224 B
```

### 3.2 新增任务的 Heap 预算

所有任务栈、TCB、队列都从现有 6,144 B `ucHeap` 分配，**不会改变链接器静态总量**。

按 V11.3.1 当前配置：

- TCB = 64 B；每次分配块头 = 8 B；`Queue_t` ≈ 72 B。
- 队列一次分配 `sizeof(Queue_t) + 队列存储`，再加 8 B 块头并向上取 8 字节对齐。
- 1 项 × 4 B 的 `xQueueOverwrite` 队列：72 + 4 + 8 = 84 → 对齐 **88 B**。
- 4 项 × 1 B 的事件队列：72 + 4 + 8 = 84 → 对齐 **88 B**。

| 方案 | 组成 | Heap 需求 | 剩余 Heap |
|---|---|---:|---:|
| 三任务（Pomodoro 128 字 + Sensor 192 字 + 两个队列） | 592 + 848 + 88 + 88 | **1,616 B** | **768 B** |
| 两任务（Sensor 192 字 + 传感器队列，Pomodoro 合并 GUI） | 848 + 88 | **936 B** | **1,448 B** |
| 两任务（Sensor 256 字 + 队列） | 1,104 + 88 | **1,192 B** | **1,192 B** |
| 单任务（全部合并 GUI） | 0 | **0 B** | **2,384 B** |

计算明细：

```text
Pomodoro 128 字：128×4 + 8 + 64 + 8 = 592 B
Sensor  192 字：192×4 + 8 + 64 + 8 = 848 B
Sensor  256 字：256×4 + 8 + 64 + 8 = 1,104 B
队列 1×4 B：72 + 4 + 8 = 84 → 88 B
```

### 3.3 静态 RAM 增量

加入 `driver_dht11.c` 后：

- `g_dht11_data_t`（2 个 `int`）= 8 B
- `DHT11_Update()` 的 `static millis_t lastUpdate` = 4 B
- 合计约 **+12 B 静态 ZI**；静态总量约 **11,268 B**，未分配约 **9,212 B**。
- 任务栈/TCB/队列不会增加这个数字，它们已在 6,144 B Heap 内。

### 3.4 Flash 增量

- 当前 Flash 35,676 / 65,536 B，余 29,860 B。
- `driver_dht11.c` 源码约 10.5 KB；编译后预计 **+1～2 KB** 代码。
- 新增任务/番茄钟模块/页面绘制预计再 +1～2 KB。
- 合计预计 **+2～4 KB**，仍余约 26 KB；**Flash 不是瓶颈**。最终值以构建后的 MAP 为准。

### 3.5 栈溢出风险

| 任务 | 建议栈 | 风险 | 说明 |
|---|---:|---|---|
| GUI | 768 字（保持） | 低 | 历史峰值 82 字；新增页面绘制后仍远低于 768 |
| Sensor | 192 字 | 低 | DHT11 调用链浅，无 printf；192 字有充足余量 |
| Pomodoro（若独立） | 128 字 | 低 | 只有计数、状态和队列发送 |

新增任务后必须重新读取 `uxTaskGetStackHighWaterMark()`；当前 686/110 字的数据只覆盖旧路径。

---

## 4. 三个任务的优先级、栈大小、执行周期建议

### 推荐方案（两任务）

| 任务 | 优先级 | 栈 | 周期/触发 | 职责 |
|---|---:|---:|---|---|
| Sensor_Task | **3** | **192 字 / 768 B** | 每 2,000 ms | DHT11 读取、失败计数、把最新数据写入队列 |
| GUI_Task | **2** | 768 字 / 3,072 B（保持） | 25–60 FPS，`vTaskDelay(1)` | OLED、菜单、动画、红外、番茄钟状态机、读取传感器队列 |
| Idle_Task | 0 | 128 字 / 512 B | 空闲 | 内核空闲 |

### 如果坚持三任务

| 任务 | 优先级 | 栈 | 周期 | 评价 |
|---|---:|---:|---|---|
| Sensor_Task | 3 | 192 字 | 2 s | 必须最高，保证 DHT11 位时序 |
| GUI_Task | 2 | 768 字 | 25–60 FPS | 保持 |
| Pomodoro_Task | 1 | 128 字 | 1 s 检查 | 只在 GUI 阻塞时运行；倒计时用 Tick 差值，不因调度延迟产生漂移 |
| Idle_Task | 0 | 128 字 | 空闲 | — |

### 为什么 GUI 不应最高

`DHT11_Read()` 的时序关键段约 4–5 ms，逐位在 26–70 µs 窗口内采样（`driver_dht11.c:188-213`）。如果 GUI 优先级高于 Sensor，GUI 每轮 `vTaskDelay(1)` 前最多运行约 24 ms 的 I2C 刷屏，Sensor 只能插在 GUI 让出的 1 ms 间隙里；逐位阶段会被反复抢占，读取失败。

两种正确做法：

1. **Sensor=3，GUI=2**：Sensor 每 2 s 抢占 GUI 约 4–24 ms，OLED 最多掉一帧；这是推荐方案。
2. **DHT11 放进 GUI**：GUI 自己在循环里完成读取，期间不切任务；代价是 GUI 自己停 4–24 ms，且代码耦合。

### 番茄钟为什么不建议独立任务

- 它只做 `deadline - now` 的减法，不阻塞、不访问外设。
- 独立任务需要 592 B 栈/TCB + 88 B 事件队列；合并后只需几个字节的静态状态。
- GUI 已经以 25–60 FPS 运行，每帧检查一次倒计时即可；1 ms Tick 精度足够。
- 如果独立任务优先级 1，它会被 GUI 的 24 ms I2C 刷屏饿住；倒计时本身用 Tick 差值不会漂移，但“结束事件”最多晚一个刷屏周期。合并到 GUI 反而更及时。

---

## 5. 任务间通信方案

### 推荐：一个传感器数据队列

- `QueueHandle_t` 1 项，item 为 `{ int8_t hum; int8_t temp; uint8_t status; }`（4 B 对齐）。
- Sensor 用 `xQueueOverwrite()` 写最新值；GUI 用 `xQueueReceive(..., 0)` 取。
- `xQueueOverwrite` 在 `queue.h:597` 有宏定义，要求队列长度为 1。
- 内存：72 + 4 + 8 = 84 → **88 B**。
- 优点：天然无锁、无优先级反转、GUI 永远拿到最新值；不需要 mutex。

### 番茄钟：合并进 GUI，不需要队列

- 状态：`idle / running / paused / finished`。
- 计时：`TickType_t deadline`；剩余 = `deadline - xTaskGetTickCount()`（无符号相减，自动处理回绕）。
- 暂停：`remaining = deadline - now`；继续：`deadline = now + remaining`。
- 结束：在 GUI 每帧检查 `now >= deadline`，置 `finished` 并刷新页面。
- 静态 RAM 约 8–12 B，无任务、无队列。

### 如果坚持三任务

- Pomodoro→GUI 用一个 4 项 × 1 B 的事件队列（88 B），或共享 `volatile` 状态 + `taskENTER_CRITICAL()` 拷贝。
- 不要用任务通知（`configUSE_TASK_NOTIFICATIONS=0`）。
- 不要用事件组（`event_groups.c` 未参与构建）。
- 不要用 mutex（`configUSE_MUTEXES=0`）；若未来要启用，会改变 TCB 大小和 Heap 账目，需要重新评估。

### 数据竞争点

| 数据 | 当前/计划 | 风险 | 处理 |
|---|---|---|---|
| `g_dht11_data_t` | 驱动全局 | Sensor 写、GUI 读会撕裂 | 不要直接共享；Sensor 读入局部变量后走队列 |
| `timeDate`、`appConfig`、`menuData` | 只在 GUI 使用 | 无 | 保持单写者 |
| `millis()` / `uwTick` | TIM4 ISR 写，任务读 | 32 位读写原子 | 无需保护 |
| `g_nwatchRtos` | GUI 写 | 无其他写者 | 保持 |
| 显示绘制函数 | 只在 GUI | 若其他任务调用会冲突 | 设计约束：Sensor/Pomodoro 禁止调用任何 `draw_*`/`LCD_*`/`OLED_*` |

---

## 6. GPIO 与外设冲突检查

### 6.1 新增功能用到的引脚

| 功能 | 引脚 | 当前占用 | 冲突结论 |
|---|---|---|---|
| DHT11 数据 | **PA1** | `main.c:557-567` 已配置开漏输出+上拉；当前无其他运行驱动使用 | 与 M2 DS18B20 **物理二选一**；确认只装 DHT11 |
| OLED | PB6/PB7（I2C1） | GUI 独占 | 不与 DHT11 冲突 |
| 红外接收 | PB10（EXTI15_10） | 已有 | 不与 DHT11 冲突 |
| 蜂鸣器（可选） | PA8 | 当前未编译；M4 红外发射也占 PA8 | **二选一**；番茄钟提示音需先确认 |
| 番茄钟 | 无 GPIO | — | 纯软件计时 |

### 6.2 详细检查

- **PA1**：板卡文档明确 M2 DS18B20 与 M10 DHT11 都接 PA1，图注“不能同时接”（`docs/board_hardware.md:18,151`）。软件上 PA1 已按 DHT11 需要的开漏+上拉配置，`driver_dht11.c:61-82` 也依赖这一配置。
- **PA1 上电电平**：`main.c:557` 在 GPIO 初始化时把 PA1 拉低；`DHT11_Init()` 会拉高（`driver_dht11.c:229-234`）。如果 Sensor 任务在调度后才初始化，PA1 会保持低一段时间；DHT11 可容忍，但建议在 `MX_GPIO_Init()` 里把初始电平改为高，或在创建任务前调用 `DHT11_Init()`。
- **上拉**：`driver_dht11.c` 注释说 PA1 已配置“open drain, pull-up”，但内部上拉约 30–50 kΩ，DHT11 的 20–40 µs 边沿可能偏慢。确认 M10 模块是否自带 4.7–10 kΩ 上拉；没有则补外部上拉。
- **PA8**：M4 红外发射与 M9 蜂鸣器共线，图注二选一（`docs/board_hardware.md:25,152`）。当前 UI-only 构建没有蜂鸣器驱动；如果番茄钟要响铃，必须新增蜂鸣器驱动并确认 M4 未接。
- **I2C1**：PB6/PB7 同时挂 M1 OLED 和 M11（型号待确认）。当前只有 GUI 访问 OLED；若未来 Sensor 通过 M11 走 I2C，必须加总线仲裁。当前没有 mutex，推荐“所有 I2C 只由 GUI 任务发起”，Sensor 只送数据。
- **PB10**：红外 EXTI 已占用，优先级 5；DHT11 用轮询，不占 EXTI。
- **PB5**：`main.c:575-579` 配置为 EXTI 上升沿，但 `stm32f1xx_it.c` 只使能了 `EXTI15_10_IRQn`，没有 `EXTI9_5_IRQn`；当前不会产生中断，也不影响 PA1。
- **TIM3/TIM4**：DHT11 的 `udelay` 依赖 TIM3 1 MHz 计数（`driver_timer.c:45-70,105-114`），`mdelay` 依赖 TIM4 HAL tick（`driver_timer.c:116-120`）。两者都已在运行，不需要新增定时器。
- **SWD**：PA13/PA14 未在 J1–J4 引出；调试口不受新增功能影响。

---

## 7. 可能存在的风险

| # | 风险 | 严重度 | 依据 | 缓解 |
|---:|---|---|---|---|
| 1 | PA1 实际接的是 DS18B20 而非 DHT11 | 高 | `docs/board_hardware.md:18,151` | 实板确认 M10/M2；只装一个 |
| 2 | DHT11 无外部上拉，读失败/偶发校验错 | 中高 | `driver_dht11.c:61-82`；板卡文档待确认 | 补 4.7–10 kΩ 上拉；连续失败计数 |
| 3 | Sensor 优先级不够，DHT11 逐位被 GUI 抢占 | 高 | `display.c:69-138`；`driver_oled.c:710-720` 约 24 ms/帧 | Sensor=3；或 DHT11 并入 GUI |
| 4 | Sensor=3 时每 2 s 抢占 GUI 20–24 ms，掉一帧 | 低 | `driver_dht11.c:132-137` 的 20 ms 起始低电平 | 把 20 ms 起始改为 `vTaskDelay`；或接受掉帧 |
| 5 | 三任务方案 Heap 只剩 768 B | 中 | 6,144 − 1,616 = 768 | 番茄钟合并 GUI；或 Sensor 栈降到 128 字 |
| 6 | 误用 mutex/事件组/任务通知导致链接失败 | 中 | `FreeRTOSConfig.h:33-38`；`event_groups.c` 未编译 | 只用队列/临界区 |
| 7 | GUI I2C 故障时单次超时 500 ms，8 页最坏 4 s | 中 | `driver_oled.c:126-127,173-176,710-720` | 保持 GUI 为 I2C 唯一拥有者；必要时加总线恢复 |
| 8 | 番茄钟结束提示无蜂鸣器 | 低 | `main.c:162-166`；构建列表 | 先做屏幕提示；要声音再评估 PA8 |
| 9 | `DHT11_Update()` 的 100 ms 周期对 DHT11 太快 | 中 | `driver_dht11.c:297-308`（注释写 10 ms，代码 100 ms） | 不要复用它；Sensor 任务每 2 s 调 `DHT11_Read()` |
| 10 | 直接共享 `g_dht11_data_t` 造成数据撕裂 | 中 | `driver_dht11.c:35,311-314` | 只传局部变量 + 队列 |
| 11 | 新增菜单页超出可视项 | 低 | `functions.h:13` `MAX_MENU_ITEMS=7`；当前 4+Exit=5 | 加 2 页后 6+Exit=7，刚好；再多需滚动/分组 |
| 12 | 加入 DHT11 驱动后 Flash/RAM 小幅增加 | 低 | 当前余 29,860 B Flash | 预计 +2～4 KB；构建后复核 |

---

## 8. 推荐的最终任务架构

### 推荐：两任务 + GUI 内番茄钟

```text
                    FreeRTOS Kernel (1 kHz SysTick)
                               |
              +----------------+----------------+
              |                                 |
        Sensor_Task (Prio 3)             GUI_Task (Prio 2)
        栈 192 字 / 768 B                栈 768 字 / 3,072 B
        每 2 s 读 DHT11                  OLED / 菜单 / 动画 / 红外
        失败计数 + 状态                  番茄钟状态机（Tick 差值）
              |                                 |
              +---- 1 项队列(4B, Overwrite) ----+
                    最新温湿度快照
                               |
                          Idle_Task (Prio 0)
```

### 不推荐：三任务平铺

```
GUI(2) + Pomodoro(1) + Sensor(3) + Idle(0)
Heap：1,616 B，剩余 768 B
```

功能上可行，但：

- 番茄钟没有阻塞点，独立任务纯粹增加调度和同步成本；
- 768 B 余量对后续页面/缓冲扩展太紧；
- 结束事件还要多一条队列或共享状态；
- 没有带来任何 UI 或精度收益。

### 任务职责边界

- **GUI_Task 独占**：OLED、I2C1、`draw_*`、`LCD_*`、`menu_*`、`timeDate`、`appConfig`、红外按键消费、番茄钟状态机。
- **Sensor_Task 独占**：PA1、`DHT11_Read()`；不调用任何显示函数；只写传感器队列。
- **ISR**：IR 只记录时间戳；TIM3 只扩展微秒计数；TIM4 只增 HAL tick；都不调用 OLED/队列以外的重活。
- **禁止**：Sensor/Pomodoro 直接调用 `OLED_*`、`LCD_*`、`draw_*`；禁止多个任务同时操作 I2C1。

---

## 9. 下一步实施顺序

> 以下只是顺序建议，本轮不执行。

1. **硬件确认**
   - 实板确认 M10 DHT11 已安装、M2 DS18B20 未安装。
   - 确认 DHT11 模块自带数据线上拉；没有则补 4.7–10 kΩ。
   - 确认番茄钟是否需要声音；若需要，确认 PA8 上接的是蜂鸣器而不是红外发射。

2. **DHT11 驱动单独入构建**
   - 在 EIDE 源文件列表加入 `Drivers/DShanMCU-F103/driver_dht11.c`。
   - 不要直接把 `NWATCH_UI_ONLY` 改为 0；只加 DHT11，避免连带引入蜂鸣器/LED/LCD 控制台。
   - 先做无 RTOS 的短测试或单任务测试，确认 PA1 读数和校验和稳定。

3. **加入 Sensor_Task**
   - 优先级 3、192 字栈、2 s 周期。
   - 1 项队列 + `xQueueOverwrite`；GUI 每帧读取。
   - 把 `DHT11_Start()` 的 20 ms `mdelay` 改为任务态 `vTaskDelay`（如果改驱动），把逐位阶段压缩到约 4–5 ms。
   - 读取 `uxTaskGetStackHighWaterMark()` 和 `xPortGetFreeHeapSize()`。

4. **番茄钟合并进 GUI**
   - 新建一个纯状态模块（不建任务）：`idle/running/paused/finished`、`TickType_t deadline`、`remaining`。
   - 复用现有红外按键映射（`buttons.c:99-117`）做开始/暂停/重置。
   - 每帧检查结束事件，先做屏幕提示。

5. **菜单与页面**
   - `m_main.c` 的 `OPTION_COUNT` 从 4 增到 6，`itemLoader` 增加“Pomodoro”“Sensor”两项；6+Exit=7，正好等于 `MAX_MENU_ITEMS`。
   - 两个页面只读 GUI 内状态和传感器队列，不直接碰 DHT11/PA1。

6. **验证与调优**
   - 连续操作菜单/立方体/CRT 动画，读取 GUI/Sensor 栈水位。
   - 记录 `xPortGetFreeHeapSize()`、`xPortGetMinimumEverFreeHeapSize()`。
   - 观察 DHT11 连续 30 分钟读取成功率、OLED 帧率、红外响应。
   - 若 GUI 掉帧明显，再把 Sensor 的 20 ms 起始段改为 `vTaskDelay`，或把采样周期从 2 s 放宽到 5 s。

---

## 10. 待确认项

1. 实板 M10 DHT11 是否安装、M2 DS18B20 是否未安装（PA1 二选一）。
2. DHT11 模块是否自带 4.7–10 kΩ 数据线上拉。
3. 番茄钟结束提示是否必须有声音；若需要，PA8 接的是蜂鸣器还是红外发射。
4. 是否接受 GUI 不是最高应用优先级（Sensor=3、GUI=2）。
5. 是否接受番茄钟合并进 GUI，而不是独立 Pomodoro_Task。
6. 未来是否会在 I2C1 上同时使用 M11 模块；若是，需要额外的总线仲裁方案（当前无 mutex）。
7. mark 架构文档的正文（本次附件未能读取）；若其中有额外约束，需要补充后再复核。

---

## 11. 本轮边界

- 未修改任何源码、头文件、启动文件、`FreeRTOSConfig.h`、EIDE 配置或 scatter 文件。
- 未创建任务、未添加驱动、未修改 OLED 驱动、未修改工程结构。
- 未重新构建、未烧录。
- 所有 RAM/Flash 数字来自当前 MAP/AXF 与 2026-11-05 的硬件快照；新增任务的数字是按 FreeRTOS V11.3.1 配置推导，最终以构建后的 MAP 和实测水位为准。
