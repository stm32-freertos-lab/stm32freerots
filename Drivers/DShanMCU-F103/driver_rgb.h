#ifndef DRIVER_RGB_H
#define DRIVER_RGB_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* 保持原有五个函数的名称和参数，新增灯效接口不影响原有调用。 */
/* 所有接口由同一个任务调用；本驱动不创建任务、不使用延时和动态内存。 */
/* 使用前仍须由原工程完成 TIM2 初始化、低有效 PWM 配置和引脚重映射。 */
typedef enum
{
    RGB_MODE_STEADY = 0, /* 常亮：使用 RGB_SetRGB/RGB_SetColor 保存的颜色。 */
    RGB_MODE_BLINK,      /* 闪烁：前半周期亮，后半周期灭。 */
    RGB_MODE_BREATHE,    /* 呼吸：保存的颜色由暗到亮，再由亮到暗。 */
    RGB_MODE_GRADIENT    /* 渐变：自动循环六段彩虹，不覆盖保存的颜色。 */
} RGB_ModeTypeDef;

HAL_StatusTypeDef RGB_Init(void);
/* 启动原有三路 PWM，返回 HAL_OK 或 HAL_ERROR；不改变定时器和 GPIO 配置。 */

void RGB_SetRGB(uint8_t red,
                uint8_t green,
                uint8_t blue);
/* 设置并保存 0~255 的三色分量，立即应用整体亮度，并切换为常亮模式。 */

void RGB_SetColor(uint32_t color);
/* 按 0xRRGGBB 设置颜色，高 8 位忽略；与 RGB_SetRGB 的行为一致。 */

void RGB_SetBrightness(uint8_t brightness);
/* 设置整体亮度：0~255；立即生效，保留原始颜色、当前模式和灯效相位。 */

void RGB_Off(void);
/* 沿用原来的清零三色逻辑，停止灯效；保留整体亮度，不自动恢复旧颜色。 */

void RGB_SetMode(RGB_ModeTypeDef mode, uint32_t periodMs);
/* 从头启动所选模式；periodMs 为完整周期，限制为 20~60000 毫秒。 */
/* 非法模式不生效。设置颜色会退出灯效，需要时先设颜色，再调用本函数。 */

void RGB_Update(void);
/* 在同一个任务的循环中反复调用，内部每隔至少 10 毫秒推进一次灯效。 */
/* 未调用本函数时，动态灯效停留在当前相位；常亮模式无需周期更新。 */

#endif /* DRIVER_RGB_H */
