#include "stm32f1xx_hal.h"
#include "sensor.h"
#include "FreeRTOS.h"
#include "task.h"
#include "driver_timer.h"
#include "driver_dht11.h"



/* 累计 DHT11 读取成功的次数，便于在调试器中观察。 */
int count=0;
 int History_Temp[TEMP_HISTORY_SIZE] = {0};
/* FreeRTOS 传感器任务入口；argument 是创建任务时传入的参数，当前未使用。 */
void Sensor_Task(void *argument)
 {
     DHT11_Init();
    /* s1 保存最近一次成功读取的数据，首次读取前先把温湿度置零。 */
    sensor_data_t s1;
    s1.hum = 0;
    s1.temp = 0;
    /* 循环前建立周期基准；后续由 vTaskDelayUntil 自动推进 lastWake。 */
    TickType_t lastWake = xTaskGetTickCount();   /* 记下当前 Tick */
    sensor_history_t s2;
    s2.temp = 0;
    s2.Writeindex = 0;
    s2.Readindex = 0;
/* 持续读取温湿度，再按 2000 ms 的固定节拍等待下一轮。 */
while(1)
{ 
     /* 通过成员地址接收结果；返回 0 表示成功，失败时不进入计数分支。 */
     if(DHT11_Read(&s1.hum,&s1.temp)==0)
{
    s2.Writeindex++;
    s2.Readindex=s2.Writeindex % TEMP_HISTORY_SIZE;
    History_Temp[Readindex]=s2.temp;
    xQueueOverwrite(g_sensorQueue, &s1);
   
    count++; /* 本次读取成功，累计次数加一。 */
}
 
 vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2000));


 }

