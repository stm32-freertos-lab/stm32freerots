# 第二阶段第一步：时间基准迁移

本轮仅迁移时间基准。未加入 FreeRTOS 内核、配置文件、任务、队列或 GUI 拆分。修改范围仅为当前 `NWatch-DShanMCU-F103-RTOS` 工程，未修改 master 备份。

## 构建结果

本轮开始时重新构建了已通过上板验证的第一阶段代码，随后使用同一 EIDE 构建器与 ARM Compiler 5.06 update 5 (build 528)、O3 完成迁移后的全量构建。

| 指标 | 迁移前 | 迁移后 | 变化 |
| --- | ---: | ---: | ---: |
| Flash 实际装载数据 | 30,228 B | 30,808 B | +580 B |
| RAM：RW + ZI | 4,816 B | 4,704 B | -112 B |
| 编译输入 | 40 个文件 | 42 个文件 | HAL TIM/TIMEx 重新启用 |

最终 `Code=22264 RO-data=8296 RW-data=248 ZI-data=4456`。0 个错误、9 个既有未使用函数警告（OLED 8 个、`pwrmgr.c:userSleep` 1 个）；原 `getRtcTime` 未使用警告已通过按 RTC 配置编译消除。

容量始终为 Flash 65,536 B / RAM 20,480 B；剩余 Flash 34,728 B、RAM 15,776 B。RAM 统计包含启动文件的主栈 1,024 B 和 C 库堆 512 B，没有 RTOS 堆或任务栈。HEX 记录校验和、实际地址范围与 map 的 Total ROM Size 已交叉核对。

第一次链接发现 HAL TIM 基础文件还引用 TIMEx 的回调，随后补齐原厂 `stm32f1xx_hal_tim_ex.c`，没有用空实现绕过依赖。

产物与记录：

- `build/NWatch-DShanMCU-F103/NWatch-DShanMCU-F103.hex`（本次待烧录版本）
- `build/NWatch-DShanMCU-F103/NWatch-DShanMCU-F103.axf`
- `phase2_timebase/build-output.log`、`verification.json`
- `phase2_timebase/baseline.hex`（迁移前已验收版本）与 `baseline.map`
- `phase2_timebase/before/`（修改前源码/配置快照）、`source-changes.diff`、`changed-sources.json`

复现命令，在 MDK-ARM 下执行：

```powershell
.\scripts\build.ps1 -LogPath phase2_timebase/build-output.log
python scripts/verify_timebase.py
```

## 原调用关系

```text
HAL_Init / HAL_RCC_ClockConfig
  -> HAL_InitTick（原 HAL 弱实现）
  -> SysTick 1ms
  -> SysTick_Handler
       -> HAL_IncTick -> HAL_GetTick -> HAL_Delay、HAL I2C/RCC 等超时
       -> systick_t++ -> SysTickGetMs -> millis_get -> GUI 帧率/动画/按键轮询
       -> update=true，每秒在 ISR 中直接增加 timeDate.time.secs

红外 PB10 双边沿 EXTI
  -> IRReceiver_IRQ_Callback
  -> system_get_ns
  -> HAL_GetTick 毫秒数 + SysTick->VAL 的毫秒内位置
```

原红外读数的两个部分并非同一原子快照，跨毫秒边界可能不一致；HAL 毫秒计数回绕后，旧纳秒拼接结果也会倒退。`udelay()` 同时直接轮询 SysTick，原 `system_get_ms()` 则始终返回 0。

## 资源核查与分配

[ST 数据手册 Table 4](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)确认，F103C8 的 TIM1/TIM2/TIM3/TIM4 均为 16 位计数器，不能把本芯片的 TIM2 当作某些其他 STM32 系列的 32 位定时器。

| 资源 | 原活动工程状态 | 本轮分配 |
| --- | --- | --- |
| TIM1 | 蜂鸣器 PWM 已关闭 | 继续保留，不启用 |
| TIM2 | 彩灯 PWM 已关闭 | 继续保留，不启用 |
| TIM3 | 活动构建未使用 | 1MHz 自由计数，65.536ms 溢出一次，软件扩展为 64 位微秒 |
| TIM4 | 活动构建未使用 | HAL 毫秒时基，默认 1ms 更新 |
| SysTick | 原 HAL/NWatch 时基 | 应用已释放；当前不启用，空入口保留给后续移植 |
| RTC | 未启用外部 RTC 方案 | 本轮未引入 |

`Core/Src/tim.c` 虽含旧 TIM3 PWM 配置，但它没有加入当前 EIDE/uVision 活动源文件集合。本轮仍不加入该文件，避免其 MSP/PWM 配置与新时基冲突。

保持现有 HSE 8MHz、PLL×9、HCLK 72MHz、APB1 36MHz、APB2 72MHz。APB1 分频不为 1 时，TIM3/TIM4 内核时钟为 2×PCLK1，本工程即 72MHz。算法按实际 RCC 时钟计算预分频，参考本机 HAL 的定时器时基模板与 RCC 实现。

- TIM4：计数频率 10kHz，PSC=7199、默认 ARR=9，产生 1ms 更新中断。HAL_Init 初期 HSI 8MHz 时 PSC=799；HAL_RCC_ClockConfig 后自动重配为最终频率，不清零 `uwTick`。同时支持 HAL 自带的 10ms/100ms 节拍设置，当前仍为 1ms。
- TIM3：PSC=71、ARR=65535，每计数一次为 1µs。系统时钟配置完成后、GPIO/红外中断及 OLED 初始化之前启动。仅使用内部时钟，不配置通道、不修改 GPIO 或重映射。
- 调试暂停时冻结 TIM3/TIM4，防止断点期间多次溢出却没有 ISR 服务。

## 新调用关系与优先级

```text
TIM4 更新中断 -> HAL_IncTick -> HAL_GetTick
                                ├ HAL_Delay、HAL 超时
                                ├ SysTickGetMs（旧名字保留）-> millis_get -> GUI/按键
                                └ time_update -> 主循环内推进表盘时间

TIM3 1MHz + 溢出扩展 -> system_get_us（64 位）
                       ├ 红外时间戳及脉冲差
                       ├ udelay
                       └ system_get_ns（兼容单位，实际分辨率仍为 1µs）

SysTick：无应用依赖
```

HAL_Init 设置的 `NVIC_PRIORITYGROUP_4` 保持不变：4 位抢占优先级、无子优先级。

| 中断 | 抢占优先级 | 工作 |
| --- | ---: | --- |
| TIM4 | 4 | 清更新标志，增加 HAL tick |
| TIM3 | 4 | 清更新标志，增加微秒扩展值 |
| EXTI15_10 / PB10 | 5（不变） | 记录时间、解析红外脉冲 |
| SysTick | 本轮不启用 | 留待 FreeRTOS 配置 |

两个计时 ISR 不执行 GUI 绘制、不修改 `timeDate`、不调用任何 RTOS API。后续添加 RTOS 时应保留它们的服务能力，并核对 BASEPRI/最大系统调用优先级；本轮没有预先添加这些配置。

强定义 `HAL_InitTick`、`HAL_SuspendTick`、`HAL_ResumeTick` 覆盖原 HAL 弱实现。暂停/恢复只操作 TIM4 更新中断，不触碰 SysTick，TIM3 微秒钟继续运行；暂停的 HAL 毫秒数不补计。`SysTickInit` 旧接口仅兼容 1ms，底层同样重配 TIM4；`SysTickDelay` 改为正常调用 HAL_Delay。

## 溢出、回绕和表盘时间

TIM3 的每次更新 ISR 为 `microOverflowUs` 增加 65,536µs。64 位扩展值与硬件 CNT 的读取在短暂 PRIMASK 临界区中完成，并恢复调用前的屏蔽状态：

1. 无溢出：扩展值 + CNT。
2. 硬件已溢出、ISR 尚未处理：检测 UIF，补一周期，重新读取 CNT；读取函数不清 UIF，随后 ISR 正常接管。
3. 读取 CNT 与检查 UIF 之间溢出：UIF 检测后重新读取 CNT，避免组合出旧低位和新高位。
4. ISR 清 UIF 和增加 64 位扩展值也共同保护，避免其他时间戳读取打断这两步。

红外的 68 项时间戳改存低 32 位微秒数，脉冲差采用无符号 32 位相减，因此可跨 65.536ms 硬件周期及约 71.58 分钟的低 32 位回绕。帧间空闲判断保留完整 64 位差值，避免长时间无按键后把多圈间隔误认为短间隔。NEC 阈值统一换算成微秒；原 4.5ms 引导高电平上限的 `55000000ns` 笔误修正为 `5500us`。

NWatch `time_update()` 改为使用无符号毫秒差推进时间，保留不足一秒的余数；即使主循环晚调用，也会补计整秒，而不是只依赖一个布尔标志。该运算可以跨约 49.71 天的 HAL tick 回绕，要求至少在一个完整 32 位毫秒周期内调用一次。时间只在主循环写入，同时处理 24 小时/12 小时显示、月份 0..11、闰年、跨年以及星期进位；保留原演示日期，星期按日期重新计算。

运行约束：软件扩展要求 TIM3 的更新至少每 65.536ms 得到一次服务；单个硬件 UIF 无法表达漏过的多个溢出。HAL 1ms tick 也要求及时服务。当前两个计时 ISR 均短小，PRIMASK 保护仅覆盖寄存器与计数快照；今后不要新增长时间全局关中断或长时间占用高优先级 ISR 的代码。普通主循环阻塞不会阻止这些中断。本轮不实现 Stop/Standby 或运行时变频后的微秒钟续接。

## 修改文件与原因

| 文件（相对工程根目录） | 修改原因 |
| --- | --- |
| `Drivers/DShanMCU-F103/driver_systick.c/.h` | TIM4 HAL 时基及暂停/恢复；保留旧毫秒 API 名字，移除重复计数变量 |
| `Drivers/DShanMCU-F103/driver_timer.c/.h` | TIM3 1MHz、64 位溢出扩展与一致快照；替换所有 SysTick 微秒延时/时间戳依赖，修复 system_get_ms |
| `Drivers/DShanMCU-F103/driver_ir_receiver.c` | 使用微秒单位与无符号差值；完整空闲差值、ISR 共享索引 volatile、时间戳缓冲边界保护；接线与按键映射不变 |
| `Core/Src/main.c` | 最终系统时钟配置后启动 TIM3 |
| `Core/Src/stm32f1xx_it.c`、`Core/Inc/stm32f1xx_it.h` | 移除 SysTick 的业务计时，加入 TIM3/TIM4 IRQ 转发 |
| `Core/Inc/stm32f1xx_hal_conf.h` | HAL TIM4 tick 优先级设为 4 |
| `NWatch/time.c/.h` | 移除 ISR 共享 update 标志，主循环推进软件钟及正确日期进位 |
| `MDK-ARM/.eide/eide.yml`、`.uvprojx` | 同步启用原厂 HAL TIM 和 TIMEx 文件 |

另调整 `MDK-ARM/scripts/build.ps1`、`build_eide.cjs` 的日志位置，以分别保留各阶段记录；新增本轮一次性修改脚本、配置脚本和静态验证脚本。

OLED/I2C 驱动、`draw.c`、`display.c`、`animation.c`、菜单算法、表盘绘制、立方体顶点、原图标资源均未修改。带有第一阶段哈希记录的 UI 文件已逐项比对一致。

## 已验证与待上板验证

已实际完成：AC5 全量编译/链接、HEX 生成、真实容量检查、EIDE/uVision 源文件集合一致性检查。map 与反汇编确认 HAL_InitTick 绑定到 driver_systick.o，新 SysTick_Handler 只有返回指令；活动应用源码不再访问 SysTick 寄存器，没有 FreeRTOS 内核/任务/堆文件。未执行硬件仿真或上板测试，以上静态检查不替代烧录验证。

请烧录新 HEX 后检查：

1. 冷启动和复位均能显示 OLED，无 RCC/I2C 超时卡死；与外部计时对照，表盘分钟推进正常。
2. 两个表盘、图标主菜单、页面切换、数字滚动、立方体和 CRT 动画，以及四档亮度/反色/旋转等显示设置正常。
3. 单击、长按连发、快速连续按红外键，保持页面可以返回；持续操作数分钟，覆盖大量 TIM3 硬件溢出。
4. 有条件时，运行超过 72 分钟再测试红外，覆盖低 32 位微秒时间戳回绕。49 天毫秒回绕已按无符号差值设计，尚未做对应时长的硬件验证。
5. HAL_SuspendTick/ResumeTick 已实现但当前 UI 不调用；本轮没有现场测试其硬件暂停/恢复行为。

目前不需要改线或增加硬件。本轮到此停止，等待用户烧录确认，不继续加入 FreeRTOS。
