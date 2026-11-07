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


#include "main.h"
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
