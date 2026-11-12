/* 显式包含 GPIO、内核和微秒计时接口，避免依赖间接声明。 */
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "driver_timer.h"
#include "env_event.h"

/* 事件组句柄只在此处定义，其他文件通过头文件引用。 */
EventGroupHandle_t g_envEventGroup = NULL;
void Env_Task(void *argument)
{
    (void)argument;

    while (1)
    {
        GPIO_PinState lightState =
            HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11);

        if (lightState == GPIO_PIN_RESET)
        {
            // 环境过暗
            xEventGroupSetBits(g_envEventGroup, ENV_DARK_BIT);
        }
        else
        {
            // 环境明亮
            xEventGroupClearBits(g_envEventGroup, ENV_DARK_BIT);
        }
        GPIO_PinState obstacleState =
            HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13);

        if (obstacleState == GPIO_PIN_RESET)
        {
            xEventGroupSetBits(g_envEventGroup, ENV_OBSTACLE_BIT);
        }
        else
        {
            xEventGroupClearBits(g_envEventGroup, ENV_OBSTACLE_BIT);
        }

        /* 仅更新光敏和避障事件，暂不接入超声波或 OLED 示例。 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool Ultrasonic_ReadDistance(float *distanceCm)
{
    uint32_t start;
    uint32_t end;
    uint32_t timeoutStart;

    if (distanceCm == NULL)
        return false;

    // 1. Trig 先拉低，等待 Echo 恢复低电平
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);

    timeoutStart = (uint32_t)system_get_us();
    while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_SET)
    {
        if ((uint32_t)(system_get_us() - timeoutStart) >= 30000U)
            return false;
    }

    // 2. Trig 输出 12us 高电平
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);

    start = (uint32_t)system_get_us();
    while ((uint32_t)(system_get_us() - start) < 12U);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);

    // 3. 等待 Echo 变成高电平
    timeoutStart = (uint32_t)system_get_us();

    while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_RESET)
    {
        if ((uint32_t)(system_get_us() - timeoutStart) >= 30000U)
            return false;
    }

    // 4. 记录 Echo 高电平开始时间
    start = (uint32_t)system_get_us();

    // 5. 等待 Echo 变成低电平
    while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_SET)
    {
        if ((uint32_t)(system_get_us() - start) >= 30000U)
            return false;
    }

    // 6. 记录 Echo 高电平结束时间
    end = (uint32_t)system_get_us();

    // 7. 根据高电平持续时间计算距离（厘米）
    *distanceCm = (float)(uint32_t)(end - start) * 0.0343f / 2.0f;

    return true;
}