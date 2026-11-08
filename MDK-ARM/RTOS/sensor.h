#ifndef SENSOR_H
#define SENSOR_H

#include "FreeRTOS.h"
#include "queue.h"
typedef struct
{
	int hum; /* 湿度整数部分，单位：%RH。 */
	int temp; /* 温度整数部分，单位：摄氏度。 */
}sensor_data_t ;

extern  QueueHandle_t g_sensorQueue ;

/* 传感器任务入口，任务创建代码由用户自行编写。 */
void Sensor_Task(void *argument);

#endif /* SENSOR_H */
