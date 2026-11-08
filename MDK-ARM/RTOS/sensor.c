#include "stm32f1xx_hal.h"
#include "sensor.h"
#include "FreeRTOS.h"
#include "task.h"
#include "driver_timer.h"
#include "driver_dht11.h"

/* 保存一次读取到的温湿度数据，两个成员都使用整数值。 */
typedef struct
{
	int hum; /* 湿度整数部分，单位：%RH。 */
	int temp; /* 温度整数部分，单位：摄氏度。 */
}sensor_data_t ;

/* 累计 DHT11 读取成功的次数，便于在调试器中观察。 */
int count=0;

/* FreeRTOS 传感器任务入口；argument 是创建任务时传入的参数，当前未使用。 */
void Sensor_Task(void *argument)
 {
     /* 任务启动时初始化一次，释放数据线，使其回到空闲高电平。 */
     DHT11_Init();
    /* s1 保存最近一次成功读取的数据，首次读取前先把温湿度置零。 */
    sensor_data_t s1;
    s1.hum = 0;
    s1.temp = 0;
    /* 循环前建立周期基准；后续由 vTaskDelayUntil 自动推进 lastWake。 */
    TickType_t lastWake = xTaskGetTickCount();   /* 记下当前 Tick */

/* 持续读取温湿度，再按 2000 ms 的固定节拍等待下一轮。 */
while(1)
{ 
     /* 通过成员地址接收结果；返回 0 表示成功，失败时不进入计数分支。 */
     if(DHT11_Read(&s1.hum,&s1.temp)==0)
{
    xQueueOverwrite(g_sensorQueue, &s1);
    /* 本次读取成功，累计次数加一。 */
    count++;
}
 
 /* 2000 ms 换算为 Tick；按上次目标时刻安排下一次唤醒，避免工作耗时累积进周期。 */
 /* 等待时让出 CPU；若目标时刻已过，可能立即返回，并非再延时完整 2 秒。 */
 vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(2000));
}   


 }

