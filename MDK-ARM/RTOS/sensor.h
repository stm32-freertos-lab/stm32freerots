#ifndef SENSOR_H
#define SENSOR_H

#include "FreeRTOS.h"
#include "queue.h"

#define TEMP_ALARM_HIGH 20
#define HUM_ALARM_HIGH  40

typedef struct
{
    int hum;  /* 湿度整数部分，单位：%RH。 */
    int temp; /* 温度整数部分，单位：摄氏度。 */
} sensor_data_t;

extern QueueHandle_t g_sensorQueue;
#define TEMP_HISTORY_SIZE 30

typedef struct
{
    uint8_t Writeindex; /* 当前索引，范围 0~TEMP_HISTORY_SIZE-1。 */
    uint8_t count;     /* 有效温度数量，范围 0~TEMP_HISTORY_SIZE */
    int History_Temp[TEMP_HISTORY_SIZE];
} sensor_history_t;

/* 传感器任务入口，任务创建代码由用户自行编写。 */
void Sensor_Task(void *argument);
void RGB_Task(void *argument);
int *History_Get(int *out, uint8_t *validCount);

#endif /* SENSOR_H */
