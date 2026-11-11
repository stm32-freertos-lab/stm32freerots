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
void RGB_SetRGB(uint8_t red, uint8_t green, uint8_t blue)
{


uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim2) + 1U;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, red * period / 255U);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, green * period / 255U);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, blue * period / 255U);

}
void RGB_Off(void)
{
    RGB_SetRGB(0U, 0U, 0U);
}

