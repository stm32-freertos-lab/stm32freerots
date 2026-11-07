# STM32 FreeRTOS 移植验收记录

验收日期：2026-11-05（Asia/Shanghai）。本次已完成 EIDE 配置核查、Clean + Rebuild、镜像交叉校验及 ST-Link 枚举。**尚不能认定 FreeRTOS 已通过硬件移植验收**：本次 STM32CubeProgrammer 明确报告 `No ST-Link detected!`，没有取得板上运行数据，没有烧录。

以下“未验证”表示缺少真实硬件证据，不表示已观察到失败。表中的静态证据不能代替运行结果。

| 检查项目 | 检查结果 | 验证方式 |
| --- | --- | --- |
| FreeRTOS 调度器 | 未验证 | 创建返回值已检查，调度器返回进入故障处理；GUI 入口已有运行状态断言。未实测 taskCreateResult=1、schedulerState=2。 |
| FreeRTOS Tick | 未验证 | 配置为 1000Hz，SysTick 连接内核，已有 Tick Hook；未读取两次持续运行快照。 |
| GUI_Task 运行 | 未验证 | 最终镜像保留 GUI 初始化、完整主循环、诊断计数和 vTaskDelay(1 tick)；未观察 CPU 实际执行。 |
| 任务切换 | 未验证 | 保留 GUI 与 Idle，未现场观察 GUI 阻塞、Idle 执行及 GUI 唤醒；单应用任务不足以充分验收应用任务间抢占。 |
| SysTick/PendSV/SVC | 未验证 | 静态连接通过：HEX 向量与 MAP 符号一致，三个入口均来自 RVDS port.o；尚未实测异常进入/返回。 |
| TIM3/TIM4 | 未验证 | 静态核查：TIM3 1MHz 微秒计时、TIM4 HAL 毫秒计时；未实测计数及中断。 |
| Heap 使用 | 未验证 | heap_4 的 ucHeap 确认为 6144B，分配失败钩子已链接；API 实际返回值未读取。 |
| 任务栈安全 | 未验证 | GUI 768 字、Idle 128 字，溢出检测模式 2 和钩子已启用；实际最小栈水位未读取。 |
| OLED/菜单/动画 | 未验证 | 原 UI、菜单、动画、立方体与资源文件本轮哈希不变且仍参与链接；未观察屏幕。 |
| 红外遥控 | 未验证 | PB10 双边沿 EXTI→回调→TIM3 时间戳链保留；未进行实物按键、连发测试。 |

## 实际构建与镜像证据

- 保持 VSCode + EIDE，AC5 5.06 update 5 build 528，Cortex-M3、O3；没有切换编译器。
- 删除前已核实绝对路径，仅清理 `MDK-ARM/build/NWatch-DShanMCU-F103`，旧 AXF/HEX/MAP 已备份到 `acceptance/before`，随后调用原有 `scripts/build.ps1` 全量重建。
- 47 个 C 文件和 1 个汇编文件全部编译。0 错误、9 个既有未使用函数警告（OLED 8 个、pwrmgr 1 个）。
- 编译输入恰含 tasks.c、queue.c、list.c、RVDS/ARM_CM3/port.c、MemMang/heap_4.c。未混入其他移植层或内存管理器。
- 967 个内核文件与工程自带 V11.3.1 发布包逐字节一致；发布包 SHA256 与已有来源清单一致。这验证本地来源一致性，不是新的在线签名认证。
- 本轮 HEX 校验和全部正确，范围为 `0x08000000..0x08008B5B`，与本轮 AXF 的 PT_LOAD 字节逐字节一致；异常向量与本轮 MAP 全部一致。
- 调度器、延时、栈水位、Heap API、故障钩子、GUI_Task 和 Idle 符号均实际链接。启动汇编中的弱默认入口被正确覆盖，无重复强定义。

| 容量项目 | 最终占用 | 容量上限/余量 |
| --- | ---: | --- |
| Flash 实际装载字节 | 35,676 B | 65,536 B；余 29,860 B |
| RAM 静态 RW + ZI | 11,256 B | 20,480 B；余 9,224 B |
| Code / RO-data | 27,344 / 8,296 B | 来自本轮 MAP |
| RW-data / ZI-data | 356 / 10,900 B | RW 初始化数据压缩后在 Flash 占 36B |
| FreeRTOS Heap | 6,144 B | 已计入静态 ZI，不可再次加到 RAM 总占用 |
| GUI 栈 | 768 个 32 位字 = 3,072 B | 从上述 RTOS Heap 动态分配 |
| Idle 栈 | 128 个 32 位字 = 512 B | 从同一 RTOS Heap 动态分配 |
| 主异常栈 / C 库堆 | 1,024 / 512 B | 已计入静态 RAM；与 RTOS Heap 独立 |

GUI 和 Idle 各分配 64B TCB；考虑本镜像 ucHeap 地址的 8 字节对齐、块头和尾哨兵，二者创建后的可用堆**静态估算为 2,384B**。这不是 xPortGetFreeHeapSize() 的实测结果。静态 RAM 余量 9,224B 也不是 RTOS 可分配堆余量。

本次未获得的值：xPortGetFreeHeapSize、xPortGetMinimumEverFreeHeapSize、GUI/Idle 实际栈水位、GUI 循环是否持续增加、Tick 是否持续增加、HardFault/assert/malloc 失败实际状态，均为未验证，详见 verification.json 中的 null。不能把未读取故障当作“无故障”。链接器给出的栈分析为 `280 bytes + Unknown`，存在函数指针等未知路径，不能用它代替运行水位验收。

## 配置、时基和阻塞分析

FreeRTOSConfig.h 已具备本次需要的功能：单核、32 位 Tick、1kHz、抢占和时间片开启、动态分配、heap_4 6KB、栈溢出检测 2、malloc 失败钩子、assert、Idle/Tick Hook、调度器状态和栈水位 API。软件定时器关闭，因此正常任务数预期为 **2（GUI + Idle）**；GUI 优先级 2，Idle 为 0。没有新增应用任务。

| 资源 | 静态配置与预期硬件值 |
| --- | --- |
| SysTick | 内核专用；72MHz / 1000Hz，LOAD=71999，CTRL 低 3 位应为 7 |
| TIM3 | 微秒计时；PSC=71、ARR=65535，溢出软件扩展为 64 位；调试暂停冻结 |
| TIM4 | HAL 毫秒计时；PSC=7199、ARR=9；调试暂停冻结 |
| NVIC | NVIC_PRIORITYGROUP_4（4 位抢占优先级）；TIM3/4=4、EXTI15_10=5、PendSV/SysTick=15 |
| BASEPRI 阈值 | configMAX_SYSCALL_INTERRUPT_PRIORITY=0x50，最低优先级编码 0xF0 |

TIM3/4 的优先级高于最大系统调用阈值，它们不调用任何 FreeRTOS API；EXTI 只记录/解析红外，也未调用 RTOS API。TIM3 的短 PRIMASK 临界区用于一致时间戳；未发现活动路径中新增长时间关中断。TIM3 更新需要在每 65.536ms 内获得服务，硬件实测仍待完成。

GUI 的 display_update 到达帧间隔前直接返回，不在该函数中等待帧时刻；每轮末尾 vTaskDelay(pdMS_TO_TICKS(1U)) 应让 GUI 进入阻塞态，Idle 获得执行机会。GUI 为无限循环，任务意外返回会由移植层 prvTaskExitError 触发 assert。

HAL_Delay 本身是按 TIM4 HAL tick 忙等待，不会主动阻塞当前 FreeRTOS 任务，也不关闭 SysTick；高优先级就绪任务仍可抢占，但低优先级 Idle 会等 GUI 让出。**当前活动镜像的 HAL_Delay、mdelay、udelay 已被链接器移除**，因此没有实际运行的 HAL_Delay 调用。

OLED 的 HAL_I2C_Master_Transmit / HAL_I2C_Mem_Write 仍为轮询阻塞调用，单次指定超时为 500ms，底层部分等待使用 HAL 内部超时。多次传输的累计时间可能更长；外设故障时，GUI 可长时间不更新，Idle 也可能暂时得不到执行，而内核 Tick 仍增加。尚不能证明实际 OLED 帧率、最大忙等待时间或不存在死锁。按限制未改造驱动和 UI。

## 本轮最小修改

只修改 RTOS/rtos_app.c 和 RTOS/rtos_app.h，添加简洁中文注释：

- `g_nwatchRtos.schedulerState`：与原诊断一起每秒采样 xTaskGetSchedulerState，正常应为 2。
- `g_nwatchRtos.taskCreateResult`：保留实际 xTaskCreate 返回值，成功应为 1；原失败处理保留。

已有 GUI/Idle 栈扫描、Heap 查询仍为每秒一次，Tick Hook 每次仅加计数，没有新增日志、串口、OLED 页面或任务。相较验收前固件，Flash 增加 80B、静态 RAM 增加 8B。145 个被跟踪源码/配置文件中只有上述两项发生变化；原 NWatch、驱动、Core、内核、EIDE 配置和启动文件未修改，没有触及 master 备份。

`g_nwatchRtos` 位于 `0x20000C68`（96B），`g_nwatchFault` 位于 `0x20000CC8`（68B）；仅适用于本次 AXF，优先使用符号。magic 应为 0x4E575254，phase=1 表示准备启动、2 表示进入 GUI、3 表示 UI 初始化完成。GUI 栈水位 `guiStackHighWaterWords` 的单位是 **32 位字**，乘 4 才是字节；它表示历史最少未使用栈，不是当前 PSP 到栈底的距离。

## ST-Link 手动运行验证

本机有 Cortex-Debug 1.12.1、GNU GDB 15.2.90（Arm GNU Toolchain 14.3.Rel1 配套）和 CubeProgrammer 2.8.0。GNU GDB 仅用于调试，固件仍由 AC5 编译。GDB 已离线读取本次 AXF 中的结构体、诊断地址、StackType_t=4B、TCB=64B；它对 AC5 压缩 RW 装载段和主机字符集给出警告，因此这只证明离线符号可读，尚未验证在线兼容性。没有现成 launch.json；在已检查的工具目录未发现已解压的 OpenOCD/GDB server，可使用本机现有 `C:\tool\vscode\xpack-openocd-0.12.0-5-win32-x64.zip`。

1. 给开发板正常供电；连接 GND、SWDIO/PA13、SWCLK/PA14 和目标电压参考，NRST 可按探针接线连接。先运行以下只读枚举，需看到实际 ST-Link 序列号，而不是只看退出码（本次无探针时 CLI 仍返回 0）：

   ```powershell
   & "$env:USERPROFILE\.eide\tools\st_cube_programer\bin\STM32_Programmer_CLI.exe" -l stlink
   ```

2. 确认板上固件与将加载的 AXF 匹配。如果需要使用本次 HEX，**烧录会覆盖开发板当前程序，须先确认后再执行 EIDE 下载**。本次没有执行该步骤。待用产物是 `build/NWatch-DShanMCU-F103/NWatch-DShanMCU-F103.hex`；调试符号必须选同目录同名 AXF，不能选根目录旧 `Nwatch-DShanMCU-F103` 子目录或 kernel-bringup 镜像。文件 SHA256 记录在 verification.json。

3. 解压上述 OpenOCD 包，将 `$openocdRoot` 设置为实际含 bin 和 scripts 的目录。以下启动服务器，不含烧录命令；包内接口使用 dapdirect_swd，不能误用 hla_swd：

   ```powershell
   & "$openocdRoot\bin\openocd.exe" -s "$openocdRoot\scripts" -f interface/stlink.cfg -c "transport select dapdirect_swd" -f target/stm32f1x.cfg
   ```

   在 MDK-ARM 另开终端启动 GDB，然后执行下面的连接和观察命令。也可在 Cortex-Debug 中配置 request=attach、servertype=openocd、相同 configFiles/transport 与匹配 AXF；不要使用 launch/download。

   ```powershell
   & 'C:\tool\vscode\bin\arm-none-eabi-gdb.exe' 'build/NWatch-DShanMCU-F103/NWatch-DShanMCU-F103.axf'
   ```

   ```text
   target extended-remote localhost:3333
   monitor halt
   source acceptance/observe.gdb
   nwatch_snapshot
   continue
   ```

   如需确认当前 Flash 与符号匹配，在已暂停的 GDB 中可运行 `compare-sections ER_IROM1`，此操作只读。AC5 压缩后的 RW 装载段不要用 GDB load 下载；若段比较失败，先解决固件/符号匹配再解释诊断变量。

4. 连续运行 2～5 秒后 Ctrl+C 暂停，再执行 nwatch_snapshot；至少重复两次，暂停期间值不增长是正常现象。预期：taskCreateResult=1、schedulerState=2、phase=3、taskCount=2；guiIterations、tickHookCount、xTickCount 和 idleHookCount 持续增加，kernelTickCount 为约每秒采样的快照。5 秒连续运行应有约 5000 个内核 Tick，允许外部手工计时和采样误差。检查 HAL 毫秒与内核 Tick 大体同速，TIM3/4 CNT/PSC/ARR 与上表相符，TIM3 扩展计数持续推进；kernelTickCount 的变化不能单独代替 SysTick ISR 观察。必要时短暂在 SysTick_Handler 设断点确认入口后立即删除，避免持续断点影响计时。

5. 先在 vTaskDelay 设临时断点确认 GUI 调用它，再在 vApplicationIdleHook 设临时断点，检查 pxCurrentTCB 为 Idle，GUI 的 TCB 位于延时列表；继续后在 NWatch_RtosPoll 设临时断点确认 GUI 再次执行。按需短暂观察 PendSV_Handler/vTaskSwitchContext。冷启动的 SVC/首次 GUI 进入可在设好临时断点后手动复位观察。删除频繁断点后再评估动画和时间。此过程证明 GUI/Idle 的基本上下文切换、阻塞及唤醒，**不能充分证明多个应用任务间的抢占/时间片/同步正确性**。

6. 连续操作至少数分钟，覆盖两个表盘、菜单进退、数字滚动、CRT 动画、立方体、亮度/反色/旋转，以及红外单击、快速连续按键和长按连发；观察无屏幕冻结、跳菜单、乱码。期间读取 freeHeapBytes、minEverFreeHeapBytes、guiStackHighWaterWords 和 idleStackHighWaterWords，记录最差值和单位。栈水位为 0 或故障 reason=3 均需判失败；水位非零也需覆盖最深 UI 路径并保留余量，不能凭启动一帧就判栈安全。

7. 观察 `g_nwatchFault.reason`：0=尚无已记录故障、1=assert、2=malloc 失败、3=栈溢出、4=GUI 创建失败、5=调度器返回、6=HardFault、7/8/9=MemManage/BusFault/UsageFault、10=NMI、11=HAL 初始化错误。非零时记录 file/line/taskName、CFSR/HFSR、stackedPC/LR；frameValid=0 时不要使用堆栈 PC 作可靠定位。已有故障函数在连接调试器时触发 BKPT 并停住。可给 NWatch_RtosFatal 设置一个断点，避免漏过故障。所有值从上电初始化后开始记录，不跨复位保存。

判定边界：只有取得上述连续运行、延时切换、内存水位和 UI/红外实测证据后，才能认定当前单 GUI 版本的基本移植成功。当前结论为“静态与构建检查通过，硬件运行验收未完成”。本轮完成后停止，不进入多任务扩展。

## 文件与复现

- `rebuild.log`：本轮 Clean 后全量构建记录。
- `verification.json`、`verify_image.py`：当前镜像、向量、容量、源码范围及未实测字段；执行 `python -X utf8 acceptance/verify_image.py` 可复核。
- `disassembly.txt`：由本轮 AXF 经 AC5 fromelf 生成；再次重建后需同步重新生成再校验。
- `stlink-probe.log`、`gdb-symbols.log`：本轮设备枚举与离线符号检查。
- `source-hashes-before.json`、`before/`：验收前源码哈希及两个诊断文件/旧镜像备份。
- `observe.gdb`：只定义观察命令，不连接、不下载、不写目标内存。

原有 scripts/verify_freertos.py 在本机 CRLF 源码的固定字符串匹配处中止，未将它报告为全部通过；历史探针日志和历史反汇编未用作本轮运行证据。该历史脚本未修改，当前验收使用上列独立脚本和本轮记录。
