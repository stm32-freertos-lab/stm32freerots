#include "driver_rgb.h"

extern TIM_HandleTypeDef htim2;

static uint8_t g_red = 0;             /* 保存原始红色分量，不保存调光后的结果。 */
static uint8_t g_green = 0;           /* 保存原始绿色分量。 */
static uint8_t g_blue = 0;            /* 保存原始蓝色分量。 */
static uint8_t g_brightness = 255;    /* 整体亮度：0 为全灭，255 为最大亮度。 */

#define RGB_UPDATE_INTERVAL_MS 10U
#define RGB_MIN_PERIOD_MS      20U
#define RGB_MAX_PERIOD_MS      60000U

static RGB_ModeTypeDef g_mode = RGB_MODE_STEADY;
static uint32_t g_effectPeriodMs = 3000U;
static uint32_t g_effectPhaseMs = 0U;
static uint32_t g_lastUpdateMs = 0U;

static void RGB_WritePWM(uint32_t period, uint32_t r, uint32_t g, uint32_t b);
/* 将已经乘过整体亮度的三色分量写入原有 PWM 通道。 */
static void RGB_Render(void);
/* 根据当前灯效相位计算输出，不改变保存的原始颜色和整体亮度。 */

HAL_StatusTypeDef RGB_Init(void)
{
    /* TIM2 的初始化、频率、极性和 GPIO 映射仍由原工程配置。 */
    if (htim2.Instance != TIM2)
        return HAL_ERROR;

    /* 启动前清零占空比，避免旧比较值导致意外亮灯。 */
    RGB_WritePWM(__HAL_TIM_GET_AUTORELOAD(&htim2) + 1U, 0U, 0U, 0U);

    /* 启动红色通道：PA2 / TIM2_CH3 */
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3) != HAL_OK)
        return HAL_ERROR;

    /* 启动绿色通道：PA15 / TIM2_CH1 */
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
        return HAL_ERROR;
    }

    /* 启动蓝色通道：PB3 / TIM2_CH2 */
    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2) != HAL_OK)
    {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
        return HAL_ERROR;
    }

    g_effectPhaseMs = 0U;
    g_lastUpdateMs = HAL_GetTick();
    RGB_Render();
    return HAL_OK;
} /* RGB_Init：启动原有三路 PWM，失败时回退已启动通道，成功后输出当前颜色。 */

void RGB_SetRGB(uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t period;
    uint32_t r;
    uint32_t g;
    uint32_t b;

    g_red = red;
    g_green = green;
    g_blue = blue;

    /* 手动设置颜色时退出灯效，保证原有调用仍然立即显示指定颜色。 */
    g_mode = RGB_MODE_STEADY;
    g_effectPhaseMs = 0U;
    g_lastUpdateMs = HAL_GetTick();

    if (htim2.Instance != TIM2)
        return;

    period = __HAL_TIM_GET_AUTORELOAD(&htim2) + 1U;
    /* 暂不除以 255，保留低亮度精度，最后统一换算为比较值。 */
    r = (uint32_t)red * g_brightness;
    g = (uint32_t)green * g_brightness;
    b = (uint32_t)blue * g_brightness;
    RGB_WritePWM(period, r, g, b);
} /* RGB_SetRGB：保存原始三色，应用整体亮度，每路只写一次比较寄存器。 */

void RGB_Off(void)
{
    RGB_SetRGB(0U, 0U, 0U);
} /* RGB_Off：沿用原逻辑清零三色，同时停止灯效；不改变整体亮度。 */

void RGB_SetColor(uint32_t color)
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    red   = (color >> 16) & 0xFF;
    green = (color >> 8)  & 0xFF;
    blue  = color & 0xFF;

    RGB_SetRGB(red, green, blue);
} /* RGB_SetColor：按 0xRRGGBB 拆分颜色，高 8 位忽略，复用原有三色设置函数。 */

void RGB_SetBrightness(uint8_t brightness)
{
    g_brightness = brightness;
    RGB_Render();
} /* RGB_SetBrightness：立即应用整体亮度，不覆盖原始颜色，也不重置灯效相位。 */

void RGB_SetMode(RGB_ModeTypeDef mode, uint32_t periodMs)
{
    if (mode != RGB_MODE_STEADY && mode != RGB_MODE_BLINK &&
        mode != RGB_MODE_BREATHE && mode != RGB_MODE_GRADIENT)
        return;

    /* 限制完整周期，防止除零，并让最短周期至少包含两次更新。 */
    if (periodMs < RGB_MIN_PERIOD_MS)
        periodMs = RGB_MIN_PERIOD_MS;
    else if (periodMs > RGB_MAX_PERIOD_MS)
        periodMs = RGB_MAX_PERIOD_MS;

    g_mode = mode;
    g_effectPeriodMs = periodMs;
    g_effectPhaseMs = 0U;
    g_lastUpdateMs = HAL_GetTick();
    RGB_Render();
} /* RGB_SetMode：从头启动所选灯效，周期单位为毫秒；非法模式保持原状态。 */

void RGB_Update(void)
{
    uint32_t nowMs;
    uint32_t elapsed;

    if (g_mode == RGB_MODE_STEADY)
        return;

    nowMs = HAL_GetTick();
    elapsed = nowMs - g_lastUpdateMs;
    if (elapsed < RGB_UPDATE_INTERVAL_MS)
        return;

    g_lastUpdateMs = nowMs;
    /* 无符号时间差可跨 Tick 回绕；延迟后直接追赶相位，不循环补帧。 */
    g_effectPhaseMs =
        (g_effectPhaseMs + elapsed % g_effectPeriodMs) % g_effectPeriodMs;
    RGB_Render();
} /* RGB_Update：由同一个任务反复调用，约每 10 毫秒推进灯效，不阻塞、不延时。 */

static void RGB_WritePWM(uint32_t period, uint32_t r, uint32_t g, uint32_t b)
{
    /* r/g/b 已乘过亮度，因此统一除以 255*255；TIM2 为 16 位定时器。 */
    /* 最大乘积为 255*255*65536，不超过 uint32_t 的表示范围。 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, r * period / 65025U);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, g * period / 65025U);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, b * period / 65025U);
} /* RGB_WritePWM：保持红 CH3、绿 CH1、蓝 CH2 映射，满亮比较值为 ARR+1。 */

static void RGB_Render(void)
{
    uint32_t period;
    uint32_t r = g_red;
    uint32_t g = g_green;
    uint32_t b = g_blue;
    uint32_t level = 255U;
    uint32_t position;
    uint32_t offset;

    if (htim2.Instance != TIM2)
        return;

    period = __HAL_TIM_GET_AUTORELOAD(&htim2) + 1U;
    switch (g_mode)
    {
        case RGB_MODE_BLINK:
            /* 一个完整周期：前半段点亮，后半段熄灭。 */
            if (g_effectPhaseMs >= g_effectPeriodMs / 2U)
                level = 0U;
            break;

        case RGB_MODE_BREATHE:
            /* 三角包络：0 -> 255 -> 0；不需要浮点运算。 */
            position = g_effectPhaseMs * 510U / g_effectPeriodMs;
            level = position <= 255U ? position : 510U - position;
            break;

        case RGB_MODE_GRADIENT:
            /* 六段插值：红 -> 黄 -> 绿 -> 青 -> 蓝 -> 紫 -> 红。 */
            position = g_effectPhaseMs * 1530U / g_effectPeriodMs;
            offset = position % 255U;
            switch (position / 255U)
            {
                case 0U: r = 255U;          g = offset;        b = 0U;            break;
                case 1U: r = 255U - offset; g = 255U;          b = 0U;            break;
                case 2U: r = 0U;            g = 255U;          b = offset;        break;
                case 3U: r = 0U;            g = 255U - offset; b = 255U;          break;
                case 4U: r = offset;        g = 0U;            b = 255U;          break;
                default: r = 255U;          g = 0U;            b = 255U - offset; break;
            }
            break;

        case RGB_MODE_STEADY:
        default:
            break;
    }

    /* 灯效包络只影响输出，绝不写回 g_red/g_green/g_blue/g_brightness。 */
    r = r * g_brightness * level / 255U;
    g = g * g_brightness * level / 255U;
    b = b * g_brightness * level / 255U;
    RGB_WritePWM(period, r, g, b);
} /* RGB_Render：统一计算常亮、闪烁、呼吸和渐变输出，亮度为零时任何模式都熄灭。 */
