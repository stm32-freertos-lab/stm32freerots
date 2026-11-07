# 第一阶段：NWatch UI 精简报告

本阶段已完成，未加入 FreeRTOS。所有修改均位于 `NWatch-DShanMCU-F103-RTOS`，没有修改 `NWatch-DShanMCU-F103-master` 备份工程。原始被修改文件保存在 `MDK-ARM/phase1/before/`；完整差异在 `source-changes.diff`。

## 实际构建结果

- 使用本机 EIDE 3.27.2 的 `unify_builder` 和原有 ARM Compiler 5.06 update 5 (build 528)。未更换编译器。
- 保持 Cortex-M3、C99、O3、函数独立节和原有标准 C 库设置。
- 修改前执行了 66 个源文件的全量构建；最终执行了 40 个源文件的全量构建，均成功。
- 最终 0 个错误、10 个警告：OLED 驱动的 8 个未使用静态函数，以及原有 `pwrmgr.c:userSleep`、`time.c:getRtcTime` 未使用警告。没有隐藏新增告警。
- EIDE 与 `.uvprojx` 的排除列表保持一致，均排除 26 个文件。实际验证过两者启用的源文件集合一致；未额外执行 uVision 构建。

| 指标（字节） | 精简前 | 精简后 | 减少 |
| --- | ---: | ---: | ---: |
| Flash 实际装载数据 | 61,308 | 30,228 | 31,080 |
| RAM：RW + ZI | 11,528 | 4,816 | 6,712 |
| RO：Code + RO data | 57,624 | 29,992 | 27,632 |

最终编译器输出：`Code=21696 RO-data=8296 RW-data=236 ZI-data=4580`。

Flash 数值取 map 的 `Total ROM Size`，并独立校验 HEX 记录校验和及地址范围。原 HEX 为 `0x08000000..0x0800EF7B`，新 HEX 为 `0x08000000..0x08007613`。原 map 的 LR_IROM1 未压缩大小为 62,452 字节，压缩后实际烧录数据为 61,308 字节；没有混用这两个口径。

容量限制始终为 Flash `0x10000` / RAM `0x5000`，未扩大容量。当前剩余 Flash 35,308 字节、RAM 15,664 字节。RAM 统计已包含启动文件保留的 1,024 字节主栈和 512 字节 C 库堆；剩余量尚未分配给 RTOS 任务与堆，也不是任务运行时栈水位测量结果。

构建产物：

- `build/NWatch-DShanMCU-F103/NWatch-DShanMCU-F103.hex`
- `build/NWatch-DShanMCU-F103/NWatch-DShanMCU-F103.axf`
- `build/NWatch-DShanMCU-F103/NWatch-DShanMCU-F103.map`
- `phase1/build-output.log`、`phase1/verification.json`

复现命令（在 MDK-ARM 下）：

```powershell
.\scripts\build.ps1
python scripts/verify_phase1.py
```

`build.ps1` 从当前 `.eide/eide.yml` 生成 EIDE 构建参数，并调用已安装的 EIDE 构建器与 AC5。VSCode 中也可直接使用原来的 EIDE 构建操作。

## 保留的界面与操作

- 原普通表盘、立方体表盘和表盘切换方式。
- 原图标菜单、横向平滑滚动、选中装饰条、页面移入/移出动画。
- 原数字滚动、表盘状态图标及图标进场效果。
- 原旋转立方体图形动画；“Tune Maker”原页面保留立方体显示，去掉音乐选择与播放，改称 `Cube Rotation`。
- 原 CRT 收拢/展开动画，新增 `CRT Animation` 菜单用于直接演示。展开定时判断在主循环中执行，避免关闭屏幕后绘图回调不再执行而无法展开。
- 原显示设置的亮度、反色、旋转、动画开关及原图标。默认启用动画；亮度统一按 0..3 循环。
- 原 `draw.c`、`display.c`、`animation.c` 及 I2C OLED 绘制/刷新驱动，没有引入 LVGL。
- 原红外读取、编码映射和三键导航：左右/加减切换，PLAY 确认；表盘下 PLAY 打开主菜单。

主菜单为 `Cube Rotation / Display / CRT Animation / About / Exit`。立方体页 PLAY 返回；CRT 和 About 页任一导航键返回；显示设置保留 Back，主菜单 Exit 返回表盘。切换动画期间屏蔽菜单重复动作，防止连续确认覆盖页面切换。

## 关闭的功能

通过 `NWatch/config.h` 的 `NWATCH_UI_ONLY=1` 和 EIDE/uVision 排除列表关闭，未删除这些应用源码：

- 三个游戏与游戏菜单。
- 闹钟及其编辑菜单、秒表、手电筒。
- 曲库、按键音、整点提示、音乐播放、声音设置。
- 时间日期编辑、休眠设置、诊断、重置等应用菜单。
- 温湿度传感器更新、彩灯反馈、启动滚动日志。
- 外部 W25Q64 配置持久化及不再需要的 SPI/UART/TIM PWM 初始化。
- 与上述功能对应的未使用图标通过条件编译排除。

配置改为 RAM 默认值：显示设置本次运行有效，复位后恢复默认值；不会读写外部 Flash。默认不自动休眠，保留 CRT 演示入口。这避免依赖原来尚未完整实现的睡眠/RTC 唤醒流程。

原有应用代码分支仍在，但恢复完整手表功能需要同时恢复构建排除列表；仅设置 `NWATCH_UI_ONLY=0` 不足以恢复完整构建。

## 源文件修改清单

路径以 RTOS 工程根目录为基准：

| 文件 | 变更 |
| --- | --- |
| `MDK-ARM/.eide/eide.yml` | 排除 26 个应用/驱动源文件，保持 AC5/O3 与 64KB/20KB 限制 |
| `MDK-ARM/NWatch-DShanMCU-F103.uvprojx` | 同步 IncludeInBuild=0 |
| `Core/Src/main.c` | 按配置关闭业务初始化/更新及非必要外设；增加 CRT 演示定时更新 |
| `NWatch/config.h` | 增加 UI 精简配置，保留动画、关闭游戏等功能 |
| `NWatch/m_main.c`、`NWatch/m_main.h` | 保留原图标菜单，配置有效入口、说明页和 CRT 演示 |
| `NWatch/menu.c` | 切换动画期间阻止重复菜单动作 |
| `NWatch/m_display.c` | 保留显示设置，关闭彩灯入口，统一亮度索引 |
| `NWatch/appconfig.c` | 精简版使用 RAM 默认配置、停止持久化、默认不休眠 |
| `NWatch/buttons.c` | 保留红外输入处理，条件关闭音效与彩灯反馈 |
| `NWatch/global.c` | 条件关闭 USB/充电声音服务 |
| `NWatch/normal.c` | 条件关闭温湿度业务绘制与传感器更新 |
| `NWatch/time.c` | 条件关闭闹钟关联和整点音效；本阶段保持原时间基准 |
| `NWatch/pwrmgr.c` | 条件关闭彩灯反馈，保留原显示电源管理接口 |
| `NWatch/resources.c` | 条件排除停用应用资源；所有原图标/字体数组内容保持不变 |
| `NWatch/watchface.c`、`NWatch/watchface.h` | 顶点表改为共享 Flash 常量 |
| `NWatch/tunemaker.c` | 复用顶点表，去掉精简版音乐业务，修正按键调整角度边界 |

在 MDK-ARM 内另新增构建/静态核查脚本、一次性修改脚本、原文件备份和本报告。没有修改硬件引脚定义、I2C 接口、OLED 驱动、红外驱动、启动文件或中断文件。

## 顶点表核查

原 `watchface.c` 的 `vertices[1440][3]` 为可写数组，确实占用 4,320 字节 RAM；`tunemaker.c` 还保留了一份完全相同的 Flash 常量副本。

现在两个页面共用 `const uint8_t cubeVertices[1440][3]`，只保留一份 Flash 数据。最终 map 中地址为 `0x080062F4`，大小仍为 4,320 字节。两处原表和新表的 SHA-256 一致：

`2ce020155668a266a2e44dcee7511827595a5b8950fc3bcf0a8d79beb703aaec`

全部 180 帧顶点数据保持不变。

## 第二阶段准备情况

内存余量已足够开始规划手动移植，但本阶段没有获取/选择 FreeRTOS 版本、添加内核或更改时间基准。

当前依赖已经核查：

| 资源 | 本阶段状态 | 第二阶段待办 |
| --- | --- | --- |
| SysTick | 仍为原 HAL 1ms 节拍；ISR 同时增加 NWatch 毫秒数和秒数 | 移交 FreeRTOS 前统一节拍设计，移走手表时间更新与 HAL 计时依赖 |
| SVC/PendSV | `stm32f1xx_it.c` 中仍有原入口 | 对接 RVDS/ARM_CM3 port，避免重复定义 |
| 红外 PB10/EXTI15_10 | 驱动与优先级 5 不变；通过 `system_get_ns()` 依赖 HAL_GetTick 和 SysTick->VAL | 先迁移微秒计时，再让 FreeRTOS 接管 SysTick |
| TIM1 | 原蜂鸣器 PWM 初始化已关闭 | 可用；本阶段未重配置 |
| TIM2 | 原彩灯 PWM 初始化已关闭 | 可考虑用于 HAL 时间基准 |
| TIM3/TIM4 | 当前活动代码未初始化 | 可考虑独立微秒计数器，供红外时间戳使用 |
| OLED | 原 I2C1 400kHz HAL 驱动、原引脚与缓冲不变 | 后续只由 GUI 任务管理 |

另外，原 `time.c` 的月份编号/进位逻辑和 ISR 共享变量需要在重做时间基准时一起校正；目前不应把表盘软件时钟视为经过长期计时验证的 RTC。

## 验证边界与上板检查

已实际执行 AC5 全量编译、链接、HEX 生成和静态产物核查；菜单返回路径已做代码检查。未运行 MCU 仿真，未连接 ST-Link 烧录，未声称完成硬件或运行时验证。

继续第二阶段前，建议用当前 HEX 验收：

1. OLED 启动、两个表盘切换、数字滚动与图标显示正常。
2. 主菜单所有入口可进入；左右循环、PLAY 确认、Back/Exit 和连续按键可正常返回。
3. 立方体动画持续运行，CRT 关闭后约一秒开始展开，且途中可以返回。
4. 四档亮度、反色、旋转和动画开关正常；重新上电恢复默认配置。

第一阶段到此停止，等待用户确认后再执行第二阶段。
