#include "sensor.h"
#include "FreeRTOS.h"
#include "driver_dht11.h"
#include "driver_timer.h"
#include "stm32f1xx_hal.h"
#include "task.h"
#include "rtos_app.h"
#include "driver_rgb.h"
/* Override the driver's weak busy-wait hook in task context.
 * The existing 20 ms start pulse still exceeds the DHT11 18 ms minimum. */


void DHT11_DelayMs(uint32_t ms) /* 使用任务延时，让 DHT11 毫秒等待期间让出 CPU。 */
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static sensor_history_t s2 = {0};
/* 累计 DHT11 读取成功的次数，便于在调试器中观察。 */

/* FreeRTOS 传感器任务入口；argument 是创建任务时传入的参数，当前未使用。 */
void Sensor_Task(void *argument) /* 周期采集温湿度，发布数据并通知 RGB 和 GUI。 */
{
    uint32_t confirmValue = 0;
    TaskHandle_t guiHandle = (TaskHandle_t)argument;
    DHT11_Init();
    /* s1 保存最近一次成功读取的数据，首次读取前先把温湿度置零。 */
    sensor_data_t s1 = {0};
    /* 循环前建立周期基准；后续由 vTaskDelayUntil 自动推进 lastWake。 */
    TickType_t lastWake = xTaskGetTickCount(); /* 记下当前 Tick */
  uint8_t alarmState = 0;
    /* 持续读取温湿度，再按 2000 ms 的固定节拍等待下一轮。 */
    while (1)
    {
    
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

            xTaskNotifyGive(rgbHandle); /* 第 0 号通知槽：提醒 RGB 读取最新温湿度。 */
           if((s1.temp > TEMP_ALARM_HIGH || s1.hum > HUM_ALARM_HIGH) && alarmState == 0U)
            {
                alarmState = 1;
                xTaskNotify(guiHandle, alarmState, eSetValueWithOverwrite);
            }
            if(xTaskNotifyWait(0, 0xFFFFFFFFUL, &confirmValue, 0) == pdTRUE)
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

int *History_Get(int *out, uint8_t *validCount) /* 在临界区复制温度历史，返回有效记录数和输出数组。 */
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
void RGB_Task(void *argument) /* 接收温湿度和定时器通知，统一管理 RGB 输出。 */
{
    sensor_data_t rgbData;
    uint8_t alarmActive = 0U;
    uint8_t rgbTimerOn = 0U; /* 新增独立的定时亮灭状态，不复用或更名原有变量。 */
    (void)argument;

    while(1)
    {
        /* 最多等待 10 ms，等待新数据期间也要推进闪烁相位。 */
        if(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10U))>0 && /* 第 0 号槽：取出新数据通知并清零计数。 */
           xQueuePeek(g_sensorQueue, &rgbData, 0) == pdTRUE)
        {
            uint8_t nextAlarmActive =
                rgbData.temp > TEMP_ALARM_HIGH || rgbData.hum > HUM_ALARM_HIGH;

            /* 仅在报警状态变化时切换模式，避免每次采样重置闪烁相位。 */
            if(nextAlarmActive != alarmActive)
            {
                alarmActive = nextAlarmActive;
                rgbTimerOn = 0U; /* 报警状态改变时重置普通亮灭相位，恢复正常后从灭灯开始。 */              
                  if(alarmActive)
                {
                    RGB_SetRGB(64U, 0U, 0U);
                    RGB_SetMode(RGB_MODE_BLINK, 500U);
                }
                else
                {
                    RGB_Off();
                }
            }
        }
        if (ulTaskNotifyTakeIndexed(1U, pdTRUE, 0) > 0) /* 第 1 号槽：非阻塞接收定时到期通知。 */
        {
            if (alarmActive == 0U) /* 保留温湿度报警优先权，报警期间不覆盖红灯闪烁。 */
            {
                rgbTimerOn = !rgbTimerOn;
                if (rgbTimerOn != 0U)
                    RGB_SetRGB(64U, 0U, 0U); /* 定时切换为红色常亮。 */
                else
                    RGB_Off(); /* 下一次定时通知切换为熄灭。 */
            }
        }
        RGB_Update();
    }
}
