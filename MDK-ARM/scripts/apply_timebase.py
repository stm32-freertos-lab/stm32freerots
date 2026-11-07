"""Migrate only the RTOS project's time bases, preserving source backups."""
from pathlib import Path
import re

mdk = Path(__file__).resolve().parent.parent
root = mdk.parent
assert root.name == 'NWatch-DShanMCU-F103-RTOS'
before = mdk/'phase2_timebase/before'
changes = {}

def read(rel):
    raw = (root/rel).read_bytes()
    changes[rel] = raw
    return raw.decode('latin1').replace('\r\n','\n')

def put(rel,s):
    p = before/rel
    p.parent.mkdir(parents=True, exist_ok=True)
    assert not p.exists(), f'Already applied: {rel}'
    p.write_bytes(changes[rel])
    (root/rel).write_bytes(s.replace('\n','\r\n').encode('latin1'))

def code(s):
    return s.encode('gbk' if rel.startswith('NWatch/') else 'utf-8').decode('latin1')

def change_func(s,name,body,ret='void'):
    pat = r'(?:static )?'+ret+r' '+name+r'\([^)]*\)\n\{.*?\n\}'
    s,n = re.subn(pat,lambda _:body,s,count=1,flags=re.S)
    assert n == 1, name
    return s

rel = 'Drivers/DShanMCU-F103/driver_systick.c'
s = read(rel)
s = s[:s.index('#include "main.h"')] + code('''#include "main.h"
#include "driver_systick.h"
#include "driver_timer.h"

static TIM_HandleTypeDef tickTimer;

/* 覆盖 HAL 弱实现：启动 HSI 和切换到 PLL 后均重配 TIM4，不占用 SysTick。 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    uint32_t timerClock = Timer_GetAPB1ClockHz();
    if(TickPriority >= (1UL << __NVIC_PRIO_BITS) || timerClock < 10000U || timerClock % 10000U != 0U)
        return HAL_ERROR;

    __HAL_RCC_TIM4_CLK_ENABLE();
    tickTimer.Instance = TIM4;
    __HAL_TIM_DISABLE_IT(&tickTimer, TIM_IT_UPDATE);
    __HAL_TIM_DISABLE(&tickTimer);

    /* 10kHz 计数；默认 ARR=9，每 1ms 更新。兼容 HAL 的 10/100ms 频率。 */
    tickTimer.Init.Prescaler = timerClock / 10000U - 1U;
    tickTimer.Init.CounterMode = TIM_COUNTERMODE_UP;
    tickTimer.Init.Period = 10U * (uint32_t)uwTickFreq - 1U;
    tickTimer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tickTimer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if(HAL_TIM_Base_Init(&tickTimer) != HAL_OK)
        return HAL_ERROR;

    /* HAL 初始化会产生 UG，清除 UIF 后再开中断，避免多计一次。 */
    __HAL_TIM_SET_COUNTER(&tickTimer, 0U);
    __HAL_TIM_CLEAR_FLAG(&tickTimer, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM4_IRQn);
    HAL_NVIC_SetPriority(TIM4_IRQn, TickPriority, 0U);
    uwTickPrio = TickPriority;
    __HAL_DBGMCU_FREEZE_TIM4();
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
    return HAL_TIM_Base_Start_IT(&tickTimer);
}

void Timebase_TickIRQHandler(void)
{
    if(__HAL_TIM_GET_FLAG(&tickTimer, TIM_FLAG_UPDATE) != RESET &&
       __HAL_TIM_GET_IT_SOURCE(&tickTimer, TIM_IT_UPDATE) != RESET)
    {
        __HAL_TIM_CLEAR_IT(&tickTimer, TIM_IT_UPDATE);
        HAL_IncTick();
    }
}

void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&tickTimer, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
    /* 暂停期间不补计；仅控制 TIM4，不影响微秒计数器或未来 RTOS 节拍。 */
    __HAL_TIM_CLEAR_FLAG(&tickTimer, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM4_IRQn);
    __HAL_TIM_ENABLE_IT(&tickTimer, TIM_IT_UPDATE);
}

/* 旧接口保留兼容；名字不变，实际毫秒来源已是 TIM4。 */
void SysTickInit(uint32_t cycle)
{
    if(cycle != CYCLE_1MS || HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK)
        Error_Handler();
}

void SysTickDelay(uint32_t m)
{
    HAL_Delay(m);
}

uint32_t SysTickGetMs(void)
{
    return HAL_GetTick();
}
''')
put(rel,s)

rel = 'Drivers/DShanMCU-F103/driver_systick.h'
s = read(rel)
s = s.replace('millis_t SysTickGetMs(void);','uint32_t SysTickGetMs(void);')
s = s.replace('extern volatile uint32_t systick_t;',code('/* TIM4 毫秒中断服务入口，供 stm32f1xx_it.c 调用。 */\nvoid Timebase_TickIRQHandler(void);'))
put(rel,s)

rel = 'Drivers/DShanMCU-F103/driver_timer.c'
s = read(rel)
s = s[:s.index('#include "driver_timer.h"')] + code('''#include "driver_timer.h"
#include "stm32f1xx_hal.h"

static TIM_HandleTypeDef microTimer;
static volatile uint64_t microOverflowUs;

uint32_t Timer_GetAPB1ClockHz(void)
{
    uint32_t clock = HAL_RCC_GetPCLK1Freq();
    /* APB1 分频不为 1 时，定时器内核时钟为 PCLK1 的两倍。 */
    if((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_HCLK_DIV1)
        clock *= 2U;
    return clock;
}

HAL_StatusTypeDef MicroTimer_Init(void)
{
    uint32_t clock = Timer_GetAPB1ClockHz();
    if(clock < 1000000U || clock % 1000000U != 0U)
        return HAL_ERROR;

    /* 只在系统时钟配置完成后初始化一次，不映射任何 GPIO/PWM 通道。 */
    __HAL_RCC_TIM3_CLK_ENABLE();
    microTimer.Instance = TIM3;
    microTimer.Init.Prescaler = clock / 1000000U - 1U;
    microTimer.Init.CounterMode = TIM_COUNTERMODE_UP;
    microTimer.Init.Period = 0xFFFFU;
    microTimer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    microTimer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if(HAL_TIM_Base_Init(&microTimer) != HAL_OK)
        return HAL_ERROR;

    microOverflowUs = 0U;
    __HAL_TIM_SET_COUNTER(&microTimer, 0U);
    __HAL_TIM_CLEAR_FLAG(&microTimer, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
    HAL_NVIC_SetPriority(TIM3_IRQn, 4U, 0U);
    __HAL_DBGMCU_FREEZE_TIM3();
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
    return HAL_TIM_Base_Start_IT(&microTimer);
}

void MicroTimer_OverflowIRQHandler(void)
{
    uint32_t primask = __get_PRIMASK();
    /* 标志清除和 64 位扩展一起保护，防止被时间戳读取打断。 */
    __disable_irq();
    if(__HAL_TIM_GET_FLAG(&microTimer, TIM_FLAG_UPDATE) != RESET &&
       __HAL_TIM_GET_IT_SOURCE(&microTimer, TIM_IT_UPDATE) != RESET)
    {
        __HAL_TIM_CLEAR_IT(&microTimer, TIM_IT_UPDATE);
        microOverflowUs += 65536ULL;
    }
    __set_PRIMASK(primask);
}

uint64_t system_get_us(void)
{
    uint32_t primask = __get_PRIMASK();
    uint32_t counter;
    uint64_t base;
    /* F103 的 TIM3 为 16 位，每 65.536ms 溢出；读取必须与扩展值一致。 */
    __disable_irq();
    base = microOverflowUs;
    counter = __HAL_TIM_GET_COUNTER(&microTimer);
    if(__HAL_TIM_GET_FLAG(&microTimer, TIM_FLAG_UPDATE) != RESET)
    {
        /* 已溢出但 ISR 尚未服务：补一周期并重新读取低位，不清标志。 */
        base += 65536ULL;
        counter = __HAL_TIM_GET_COUNTER(&microTimer);
    }
    __set_PRIMASK(primask);
    return base + counter;
}

void udelay(int us)
{
    uint64_t start;
    if(us <= 0)
        return;
    start = system_get_us();
    while(system_get_us() - start < (uint32_t)us)
    {
    }
}

void mdelay(int ms)
{
    if(ms > 0)
        HAL_Delay((uint32_t)ms);
}

uint64_t system_get_ns(void)
{
    /* 兼容旧接口的单位，实际分辨率为 1us，不依赖 HAL 毫秒计数。 */
    return system_get_us() * 1000ULL;
}

uint32_t system_get_ms(void)
{
    return HAL_GetTick();
}
''')
put(rel,s)

rel = 'Drivers/DShanMCU-F103/driver_timer.h'
s = read(rel)
s = s.replace('#include <stdint.h>', '#include <stdint.h>\n#include "stm32f1xx_hal.h"\n\n'+code('/* TIM3 独立微秒计时：在 SystemClock_Config 后、启用红外中断前初始化。 */\n')+'''uint32_t Timer_GetAPB1ClockHz(void);
HAL_StatusTypeDef MicroTimer_Init(void);
void MicroTimer_OverflowIRQHandler(void);
uint64_t system_get_us(void);''',1)
put(rel,s)

rel = 'Core/Inc/stm32f1xx_hal_conf.h'
s = read(rel)
s,n = re.subn(r'#define  TICK_INT_PRIORITY[^\n]*',code('/* TIM4 高于红外 EXTI 的优先级 5；ISR 不调用 RTOS API。 */\n#define  TICK_INT_PRIORITY            4U'),s,count=1)
assert n==1
put(rel,s)

rel = 'Core/Src/main.c'
s = read(rel)
s = s.replace('/* USER CODE BEGIN Includes */', '/* USER CODE BEGIN Includes */\n#include "driver_timer.h"',1)
s = s.replace('/* Reset of all peripherals, Initializes the Flash interface and the Systick. */','/* Reset peripherals and initialize the HAL TIM4 millisecond time base. */')
s = s.replace('  /* USER CODE BEGIN SysInit */', '  /* USER CODE BEGIN SysInit */\n'+code('  /* TIM3 在最终 72MHz 时钟下启动，先于红外 EXTI 和 OLED 初始化。 */\n')+'''  if(MicroTimer_Init() != HAL_OK)
    Error_Handler();
''',1)
put(rel,s)

rel = 'Core/Src/stm32f1xx_it.c'
s = read(rel)
s = s.replace('/* USER CODE BEGIN Includes */','/* USER CODE BEGIN Includes */\n#include "driver_timer.h"',1)
s = change_func(s,'SysTick_Handler',code('''void SysTick_Handler(void)
{
  /* 已释放给未来 FreeRTOS；当前不启用、不承担 HAL 或 NWatch 计时。 */
}'''))
s = s.replace('/* USER CODE BEGIN 1 */',code('''/* USER CODE BEGIN 1 */
void TIM3_IRQHandler(void)
{
  MicroTimer_OverflowIRQHandler();
}

void TIM4_IRQHandler(void)
{
  Timebase_TickIRQHandler();
}
'''),1)
put(rel,s)

rel = 'Core/Inc/stm32f1xx_it.h'
s = read(rel)
s = s.replace('/* USER CODE BEGIN EFP */','/* USER CODE BEGIN EFP */\nvoid TIM3_IRQHandler(void);\nvoid TIM4_IRQHandler(void);',1)
put(rel,s)

rel = 'Drivers/DShanMCU-F103/driver_ir_receiver.c'
s = read(rel)
s = s.replace('static int g_KeysBuf_R, g_KeysBuf_W;', 'static volatile int g_KeysBuf_R, g_KeysBuf_W;')
s = s.replace('static uint64_t g_IRReceiverIRQ_Timers[68];',code('/* 保存低 32 位微秒时间戳；无符号相减可跨 71.58 分钟回绕。 */\nstatic uint32_t g_IRReceiverIRQ_Timers[68];'))
s = s.replace('uint64_t time;', 'uint32_t time;')
s = s.replace('static uint64_t pre_time = 0;', 'static uint32_t pre_time = 0;')
s = s.replace('time = system_get_ns();', 'time = (uint32_t)system_get_us();')
# Unit conversion, retaining NEC decoding and key mapping.
for old,new in [('8000000','8000U'),('10000000','10000U'),('3500000','3500U'),
                ('55000000','5500U'),('1000000','1000U'),('2000000','2000U'),
                ('2500000','2500U'),('100000000','100000U')]:
    s = re.sub(r'\b'+old+r'\b',new,s)
s = s.replace('    uint32_t time;\n    static uint32_t pre_time = 0;', '    uint32_t time;\n    uint64_t now;\n    static uint64_t pre_time = 0;',1)
s = s.replace('time = (uint32_t)system_get_us();', 'now = system_get_us();\n    time = (uint32_t)now;',1)
s = s.replace('    if (time - pre_time > 100000U)', code('    /* 帧间空闲用完整 64 位值，长时间无按键也不会因低位回绕误判。 */\n')+'    if (now - pre_time > 100000ULL)')
s = s.replace('    pre_time = time;', '    pre_time = now;\n\n'+code('    /* 噪声或中断恢复后也不能越过 68 项时间戳缓冲。 */\n')+'    if(g_IRReceiverIRQ_Cnt >= 68)\n        g_IRReceiverIRQ_Cnt = 0;',1)
s = s.replace('/* PA10', '/* PB10')
put(rel,s)

rel = 'NWatch/time.c'
s = read(rel)
s = s.replace(' bool update;',code('/* 软件表盘只在主循环更新，不再由中断直接修改 timeDate。 */\nstatic millis_t lastSecondMs;\nstatic bool clockRunning;'))
s = s.replace('static void getRtcTime(void);', '#ifdef RTC_SRC\nstatic void getRtcTime(void);\n#endif')
# Compute weekday only after all date fields are initialized.
s = s.replace('\ttimeDate.time.ampm = CHAR_24;', '\ttimeDate.date.day = time_dow(timeDate.date.year, timeDate.date.month, timeDate.date.date);\n\ttimeDate.time.ampm = CHAR_24;',1)
s = change_func(s,'time_sleep',code('''void time_sleep()
{
    /* 普通休眠不停止软件钟；唤醒后的主循环按已过毫秒补计。 */
    time_update();
}'''))
s = change_func(s,'time_shutdown','''void time_shutdown()
{
    time_update();
    clockRunning = false;
}''')
s = change_func(s,'time_wake','''rtcwake_t time_wake()
{
#ifdef RTC_SRC
    getRtcTime();
#endif
    if(!clockRunning)
        lastSecondMs = millis_get();
    clockRunning = true;
    return RTCWAKE_SYSTEM;
}''',ret='rtcwake_t')
s = s.replace('\ttimeDate.time.secs = 0;', '\ttimeDate.time.secs = 0;\n\tlastSecondMs = millis_get();',1)
s = change_func(s,'time_update',code('''void time_update()
{
    millis_t now = millis_get();
    uint32_t elapsed = (millis_t)(now - lastSecondMs) / 1000U;
    uint32_t total;
    uint32_t daysElapsed;
    time_s time;

    if(!clockRunning || elapsed == 0U)
        return;
    /* 无符号差值跨 32 位毫秒回绕；保留不足一秒部分，不丢失延迟更新。 */
    lastSecondMs += elapsed * 1000U;
#ifdef RTC_SRC
    getRtcTime();
#else
    time = timeDate.time;
    time_timeMode(&time, TIMEMODE_24HR);
    total = (uint32_t)time.hour * SECONDS_IN_HOUR +
            (uint32_t)time.mins * SECONDS_IN_MIN + time.secs + elapsed;
    daysElapsed = total / SECONDS_IN_DAY;
    total %= SECONDS_IN_DAY;
    time.hour = total / SECONDS_IN_HOUR;
    time.mins = (total % SECONDS_IN_HOUR) / SECONDS_IN_MIN;
    time.secs = total % SECONDS_IN_MIN;
    time_timeMode(&time, appConfig.timeMode);
    timeDate.time = time;

    /* 月份数组为 0..11，日期进位与闰年使用同一编号约定。 */
    while(daysElapsed-- != 0U)
    {
        if(++timeDate.date.date > time_monthDayCount(timeDate.date.month, timeDate.date.year))
        {
            timeDate.date.date = 1;
            if(++timeDate.date.month >= 12)
            {
                timeDate.date.month = MONTH_JAN;
                if(++timeDate.date.year == 100)
                    timeDate.date.year = 0;
            }
        }
        timeDate.date.day = (day_t)(((byte)timeDate.date.day + 1U) % 7U);
    }
#endif
#if !NWATCH_UI_ONLY
    if(timeDate.time.secs == 0 && timeDate.time.mins == 0)
        tune_play(tuneHour, VOL_HOUR, PRIO_HOUR);
#endif
}'''))
# There is no fallback that derives calendar fields from milliseconds.
a=s.index('static void getRtcTime()\n')
s=s[:a]+'''#ifdef RTC_SRC
static void getRtcTime()
{
    rtc_get(&timeDate);
    timeDate.time.ampm = CHAR_24;
    time_timeMode(&timeDate.time, appConfig.timeMode);
}
#endif
'''
put(rel,s)

rel = 'NWatch/time.h'
s = read(rel).replace('extern bool update;','')
put(rel,s)

print('Modified',len(changes),'source files (backed up under phase2_timebase/before).')
