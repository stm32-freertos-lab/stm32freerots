#ifndef ENV_EVENT_H
#define ENV_EVENT_H

#include <stdbool.h> /* 为测距接口提供 bool 类型。 */
#include "FreeRTOS.h"
#include "event_groups.h"

/* 环境状态事件位 */
#define ENV_DARK_BIT        (1U << 0)
#define ENV_OBSTACLE_BIT    (1U << 1)
#define ENV_NEAR_BIT        (1U << 2)
#define ENV_SENSOR_ERROR_BIT (1U << 3)

/* 所有环境事件 */
#define ENV_ALL_BITS \
    (ENV_DARK_BIT | ENV_OBSTACLE_BIT | \
     ENV_NEAR_BIT | ENV_SENSOR_ERROR_BIT)

extern EventGroupHandle_t g_envEventGroup;
void Env_Task(void *argument);
bool Ultrasonic_ReadDistance(float *distanceCm);

#endif