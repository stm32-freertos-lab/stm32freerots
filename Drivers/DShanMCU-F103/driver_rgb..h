#ifndef DRIVER_RGB_H
#define DRIVER_RGB_H



#include "stm32f1xx_hal.h"
#include <stdint.h>

/* RGB 初始化 */
HAL_StatusTypeDef RGB_Init(void);

/* 设置 RGB 三路颜色 */
void RGB_SetRGB(uint8_t red,
                uint8_t green,
                uint8_t blue);

/* 使用 24 位颜色值设置颜色 */
void RGB_SetColor(uint32_t color);

/* 设置整体亮度：0~255 */
void RGB_SetBrightness(uint8_t brightness);

/* 关闭 RGB */
void RGB_Off(void);

#endif // !DRIVER_RGB_H