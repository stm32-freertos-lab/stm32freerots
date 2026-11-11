#include "driver_rgb.h"

extern TIM_HandleTypeDef htim2;

HAL_StatusTypeDef RGB_Init(void)
{
    /* 启动红色通道：PA2 / TIM2_CH3 */
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3) != HAL_OK)
        return HAL_ERROR;

    /* 启动绿色通道：PA15 / TIM2_CH1 */
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
        return HAL_ERROR;
    }

    /* 启动蓝色通道：PB3 / TIM2_CH2 */
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2) != HAL_OK)
    {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
        return HAL_ERROR;
    }

    return HAL_OK;
}


   
