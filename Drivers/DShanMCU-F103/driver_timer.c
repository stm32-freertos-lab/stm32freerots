// SPDX-License-Identifier: GPL-3.0-only
/*
 * Copyright (c) 2008-2023 100askTeam : Dongshan WEI <weidongshan@qq.com> 
 * Discourse:  https://forums.100ask.net
 */
 
/*  Copyright (C) 2008-2023 深圳百问网科技有限公司
 *  All rights reserved
 *
 * 免责声明: 百问网编写的文档, 仅供学员学习使用, 可以转发或引用(请保留作者信息),禁止用于商业用途！
 * 免责声明: 百问网编写的程序, 可以用于商业用途, 但百问网不承担任何后果！
 * 
 * 文件名称：driver_color_led.c
 * 本程序遵循GPL V3协议, 请遵循协议
 * 百问网学习平台   : https://www.100ask.net
 * 百问网交流社区   : https://forums.100ask.net
 * 百问网官方B站    : https://space.bilibili.com/275908810
 * 本程序所用开发板 : DShanMCU-F103
 * 百问网官方淘宝   : https://100ask.taobao.com
 * 联系我们(E-mail): weidongshan@qq.com
 *
 *          版权所有，盗版必究。
 *  
 * 修改历史     版本号           作者        修改内容
 *-----------------------------------------------------
 * 2023.08.04      v01         百问科技      创建文件
 *-----------------------------------------------------
 */

#include "driver_timer.h"
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
