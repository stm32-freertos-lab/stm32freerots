# USART1 串口测试清理报告

日期：2026-11-11（Asia/Shanghai）。

## 检查基线

- 修改前 HEAD 为 6f55daa；工作区已有 Core/Src/main.c 和 Drivers/DShanMCU-F103/driver_rgb.c 两个未提交修改。本次保留这些文件中与 RGB 有关的开发内容，没有执行 reset、revert、checkout 或回退整个提交。
- 对照测试前提交 da86f4f（2026-11-10 16:01），核对 7795e78（11-11 10:13）、185d634（10:45）和后续修补 089610b（15:07）。
- 7795e78 将原有 USART1 句柄、初始化声明/实现/调用移出了 UI 条件编译；同一提交还包含有效的蜂鸣器、GUI 和工程修改，均予以保留。
- 185d634 新增单字节接收变量、调试变量、接收启动、接收完成回调及 USART1 中断处理。089610b 修补中断位置并新增 USART1 NVIC 优先级/使能。
- 修改开始时，用户已在工作区删除 HAL_UART_RxCpltCallback()，本次保留这一删除；没有恢复 HEAD 中含多余字符 D 的旧回调。

## 文件和代码改动

| 文件 | 处理 |
| --- | --- |
| Core/Src/main.c | 删除 uartRxByte、g_uartLastByte、HAL_UART_Receive_IT() 接收启动；保留已删除回调的状态。将 huart1 及 MX_USART1_UART_Init() 的声明、实现、调用恢复到原有 !NWATCH_UI_ONLY 条件内。保留 driver_rgb.h 引用及现有 TIM2 初始化调用。 |
| Core/Src/stm32f1xx_it.c | 删除本次新增的 huart1 外部引用和调用 HAL_UART_IRQHandler() 的 USART1_IRQHandler()。文件内容恢复到测试前基线。 |
| Core/Src/stm32f1xx_hal_msp.c | 删除本次新增的 USART1_IRQn 优先级 6 配置及中断使能。原有 UART 时钟、PA9/PA10 MSP 初始化和反初始化保留；文件内容恢复到测试前基线。 |

没有删除任何源码或 HAL 库文件。Clean 仅删除经路径核验的 MDK-ARM/build/NWatch-DShanMCU-F103 生成目录（130 个生成文件），随后重新生成。

修改前构建还发现现有 RGB 开发内容中的 MX_TIM2_Init() 调用已移出条件编译，但其句柄、声明和实现仍被条件屏蔽，导致一个未定义符号。为保留该调用，本次只同步 htim2、MX_TIM2_Init() 声明/实现的编译范围。TIM2 PWM 函数体、预分频 359、周期 1999、三个通道，以及 PA2/PA15/PB3 和部分重映射配置均保持原内容。没有增加 RGB 功能或修改 RGB 驱动实现。

## 保留的 UART 配置与原因

- 原有 main.c 中 115200、8 位、1 停止位、无校验、TX/RX、无硬件流控的 USART1 初始化：测试前已存在，恢复原有条件编译，当前 UI 构建不启用。
- 原有 HAL_UART_MspInit()/HAL_UART_MspDeInit() 的 USART1 时钟和 PA9/PA10 配置：测试前已存在，本次仅去掉新增 NVIC 配置。新镜像链接时这些未使用的 MSP 段已被移除。
- Core/Src/usart.c、Core/Inc/usart.h、Core/Src/main - Copy.c 和 driver_uart.c/h：原有源码，当前 EIDE 源文件清单未启用这些 UART 文件，保留原状。
- NWatch-DShanMCU-F103.ioc、HAL_UART_MODULE_ENABLED 和 HAL UART/USART 库：原有配置与公共库，完整保留。
- startup_stm32f103xb.s 的 USART1 向量及弱符号：保留原始向量表布局。当前指向启动文件默认弱处理入口，没有应用级接收处理；未使能 USART1 NVIC。
- EIDE 的 UART 库和 driver_uart.c 排除项保持修改前状态，没有改动 EIDE/AC5 工程配置。

## 验证结果

- 执行全局搜索 USART1、HAL_UART、uartRxByte、g_uartLastByte，包含隐藏文件和被 Git 忽略的文件。完整结果在 [search-full.txt](../MDK-ARM/build/usart1-cleanup/search-full.txt)。当前应用源码中没有接收测试变量、接收启动、用户接收回调、HAL_UART_IRQHandler() 调用或 USART1_IRQn NVIC 配置；搜索剩余命中属于原有配置、公共 HAL/CMSIS 库、历史归档、构建记录、修改前快照或本报告。
- git diff --check 通过。stm32f1xx_it.c 和 stm32f1xx_hal_msp.c 与 da86f4f 的 Git 内容完全一致。
- 修改前完整重建失败：HAL_UART_Init、HAL_UART_Receive_IT、HAL_UART_IRQHandler 和 MX_TIM2_Init 四个未定义符号；共 10 个编译警告。原始日志在 [before-rebuild.log](../MDK-ARM/build/usart1-cleanup/before-rebuild.log)。
- 清理生成目录后，使用工程现有 scripts/build_eide.cjs、EIDE unify_builder 3.11.1 与 ARM Compiler 5.06 update 5 (build 528) 执行完整重建：52 个 C 文件和 1 个汇编文件，退出码 0，0 个错误，9 个原有未使用函数警告（OLED 驱动 8 个，pwrmgr 1 个），没有新增警告。日志在 [clean-rebuild.log](../MDK-ARM/build/usart1-cleanup/clean-rebuild.log)。
- 新镜像成功生成 AXF/HEX/S19。Code=35912，RO-data=8488，RW-data=396，ZI-data=11292；总 RAM 使用 11688 字节，位于原有 20 KB 配置内。
- 核对 MAP、fromelf 符号/反汇编和 HEX 实际向量内容：USART1 指向 startup_stm32f103xb.o 默认弱入口 0x08000293；SVC、PendSV、SysTick 仍指向 FreeRTOS port；TIM3、TIM4 和红外 EXTI15_10 向量与各自处理函数地址一致。
- 当前固件中没有 huart1、uartRxByte、g_uartLastByte、HAL_UART_Init、HAL_UART_Receive_IT、HAL_UART_RxCpltCallback 或 HAL_UART_IRQHandler 链接符号；无未定义引用。
- 内容快照/哈希核对通过：driver_rgb.c/h、NWatch 页面/菜单/动画、RTOS/传感器代码、DHT11、红外驱动、HAL 库、.ioc、EIDE/AC5 和 ST-Link/Cortex Debug/SWD 配置未被本次修改。main.c 中 GUI、系统时钟、各定时器配置和 GPIO 等原有函数体保持一致；TIM1/TIM3/TIM4 原有用途保留。
- driver_rgb.c 当前仍未加入现有 EIDE 源文件清单，本次保持这项开发状态，未擅自集成新 RGB 行为。

上述验证覆盖源码保留、编译/链接和镜像中断向量；未烧录或上板运行，OLED 交互、红外遥控、DHT11 采集及温湿度报警的实际运行尚未实测。

[机器可读核验结果](../MDK-ARM/build/usart1-cleanup/verification.json)；修改前源码快照位于 MDK-ARM/build/usart1-cleanup/before/。构建证据目录被工程原有 .gitignore 忽略。
