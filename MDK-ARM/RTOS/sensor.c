#include "sensor.h"
#include "FreeRTOS.h"
#include "driver_dht11.h"
#include "driver_timer.h"
#include "stm32f1xx_hal.h"
#include "task.h"
#include "rtos_app.h"
/* Override the driver's weak busy-wait hook in task context.
 * The existing 20 ms start pulse still exceeds the DHT11 18 ms minimum. */
uint32_t confirmValue = 0;

void DHT11_DelayMs(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static sensor_history_t s2 = {0};
/* 累计 DHT11 读取成功的次数，便于在调试器中观察。 */

/* FreeRTOS 传感器任务入口；argument 是创建任务时传入的参数，当前未使用。 */
void Sensor_Task(void *argument)
{
    TaskHandle_t guiHandle = (TaskHandle_t)argument;
    DHT11_Init();
    /* s1 保存最近一次成功读取的数据，首次读取前先把温湿度置零。 */
    sensor_data_t s1 = {0};
    /* 循环前建立周期基准；后续由 vTaskDelayUntil 自动推进 lastWake。 */
    TickType_t lastWake = xTaskGetTickCount(); /* 记下当前 Tick */

    /* 持续读取温湿度，再按 2000 ms 的固定节拍等待下一轮。 */
    while (1)
    {
      uint8_t alarmState = 0;
        /* 通过成员地址接收结果；返回 0 表示成功，失败时不进入计数分支。 */
        uint32_t startedUs = (uint32_t)system_get_us();
        int result;
        g_nwatchRtos.sensorReadActive = 1U;
        result = DHT11_Read(&s1.hum, &s1.temp);
        g_nwatchRtos.sensorReadLastUs = (uint32_t)system_get_us() - startedUs;
        if(g_nwatchRtos.sensorReadLastUs > g_nwatchRtos.sensorReadMaxUs)
            g_nwatchRtos.sensorReadMaxUs = g_nwatchRtos.sensorReadLastUs;
        g_nwatchRtos.sensorReadResult = result;
        ++g_nwatchRtos.sensorReadCount;
        g_nwatchRtos.sensorReadActive = 0U;
        if (result == 0)
        {
            taskENTER_CRITICAL();
            s2.History_Temp[s2.Writeindex] = s1.temp;
            s2.Writeindex = (s2.Writeindex + 1) % TEMP_HISTORY_SIZE;

            if (s2.count < TEMP_HISTORY_SIZE)
                s2.count++; /* 本次读取成功，累计次数加一。 */
            taskEXIT_CRITICAL();
            /* The queue protects itself; do not mask IR for its bookkeeping. */
            xQueueOverwrite(g_sensorQueue, &s1);
            if(s1.temp > TEMP_ALARM_HIGH || s1.hum > HUM_ALARM_HIGH)
            {
                alarmState = 1;
                xTaskNotify(guiHandle, alarmState, eSetValueWithOverwrite);
            }
            if(xTaskNotifyWait(0, 0xFFFFFFFFUL, &confirmValue, portMAX_DELAY) == pdTRUE)
{
    if(confirmValue == 2U)
    {
        alarmState = 0;
        confirmValue = 0;
    }
}
        }
        g_nwatchRtos.sensorStackHighWaterWords = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2000));
    }

}

int *History_Get(int *out, uint8_t *validCount)
{

    if (out == NULL || validCount == NULL)
    {
        return 0;
    }

    else
    {
        uint32_t startedUs = (uint32_t)system_get_us();
        uint32_t elapsedUs;
        taskENTER_CRITICAL();
        *validCount = s2.count;
        for (int i = 0; i < s2.count; i++)
        {
            int History_count = (s2.Writeindex + TEMP_HISTORY_SIZE - s2.count + i) % TEMP_HISTORY_SIZE;
            out[i] = s2.History_Temp[History_count];
        }
        taskEXIT_CRITICAL();
        elapsedUs = (uint32_t)system_get_us() - startedUs;
        if(elapsedUs > g_nwatchRtos.historyCopyMaxUs)
            g_nwatchRtos.historyCopyMaxUs = elapsedUs;
        return out;
    }
}
