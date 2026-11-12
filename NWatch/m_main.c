/*
 * Project: N|Watch
 * Author: Zak Kemble, contact@zakkemble.co.uk
 * Copyright: (C) 2013 by Zak Kemble
 * License: GNU GPL v3 (see License.txt)
 * Web: http://blog.zakkemble.co.uk/diy-digital-wristwatch/
 */

#include "common.h"

#if NWATCH_UI_ONLY
#include "sensor.h"
#include "env_event.h" /* 环境页面只读取事件组，不参与传感器采集。 */

/* 保留原菜单顺序，在温湿度页后追加环境监测入口。 */
#define OPTION_COUNT 6
#define TEMP_MENU_OPTION 4
#define ENV_MENU_OPTION 5

/* 32x32, vertical-page 1bpp: thermometer + droplet, shared with the carousel. */
static const byte tempMenuIcon[128] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xFC, 0x0C, 0x06, 0x06, 0x06, 0x06, 0x06, 0x0C, 0xFC, 0xF8,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xE0, 0xF0, 0xE0, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFC, 0xFC, 0xFC, 0x00, 0x00, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x80, 0xE0, 0x78, 0x1E, 0x07, 0x01, 0x00, 0x01, 0x07, 0x1E, 0x78, 0xE0, 0x80,
    0x00, 0x00, 0x00, 0xC0, 0xC0, 0x7F, 0x7F, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x7F, 0x7F,
    0xC0, 0xC0, 0x00, 0x1F, 0x7F, 0xE0, 0xC0, 0x9F, 0x0F, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0x7F, 0x1F,
    0x00, 0x00, 0x00, 0x07, 0x1F, 0x38, 0x60, 0x70, 0xD8, 0xDF, 0xDF, 0xDF, 0xD8, 0x70, 0x60, 0x38,
    0x1F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x03, 0x03, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00,
};

/* Six-pixel corner arms. Each bitmap has the draw_bitmap-required 8-row height. */
static const byte tempMenuCorners[4][6] = {
    {0x1F, 0x01, 0x01, 0x01, 0x01, 0x01},
    {0x01, 0x01, 0x01, 0x01, 0x01, 0x1F},
    {0xF8, 0x80, 0x80, 0x80, 0x80, 0x80},
    {0x80, 0x80, 0x80, 0x80, 0x80, 0xF8}
};

/* 32×32 菜单图标：四行状态的监测屏，沿用原轮播位图格式。 */
static const byte envMenuIcon[128] = {
    0x00, 0x00, 0xFC, 0x04, 0x04, 0x64, 0x64, 0x64, 0x04, 0x04, 0x04, 0x24, 0x24, 0x24, 0x24, 0x24,
    0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x04, 0x04, 0x04, 0xFC, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0x8C, 0x8C, 0x8C, 0x00, 0x00, 0x00, 0x84, 0x84, 0x84, 0x84, 0x84,
    0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x80, 0x80, 0xB1, 0xB1, 0xB1, 0x80, 0x80, 0x80, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x80, 0x80, 0x80, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x1F,
    0x1F, 0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* 8×8 单色列位图：光线、障碍物、测距声波、系统警示。 */
static const byte envStatusIcons[4][8] = {
    {0x08, 0x22, 0x1C, 0x5D, 0x1C, 0x22, 0x08, 0x00},
    {0x08, 0x08, 0x08, 0x2A, 0x1C, 0x00, 0x7F, 0x00},
    {0x1C, 0x41, 0x22, 0x1C, 0x41, 0x22, 0x1C, 0x00},
    {0x7E, 0x81, 0x81, 0xAD, 0xAD, 0x81, 0x81, 0x7E}
};

static void envOpen(void);
static display_t envDraw(void);
static bool envBack(void);
static display_t mMainDraw(void);
static display_t tempMenuDraw(void);
static void aboutOpen(void);
static display_t aboutDraw(void);
static bool aboutBack(void);
static void tempOpen(void);
static display_t tempDraw(void);
static bool tempBack(void);
static void crtDemoOpen(void);
static display_t crtDemoDraw(void);
static bool crtDemoBack(void);
static millis_t crtDemoStarted;
static bool crtDemoOpening;
static bool crtDemoActive;
#else
#define OPTION_COUNT getItemCount()
#endif

static prev_menu_s prevMenuData;

static void mOpen(void);
static void mSelect(void);
static void itemLoader(byte);

#if !NWATCH_UI_ONLY
static uint8_t getItemCount()
{
  uint8_t cnt = 2;
#if COMPILE_GAME1 || COMPILE_GAME2 || COMPILE_GAME3
  ++cnt;
#endif
#if COMPILE_STOPWATCH
  ++cnt;
#endif
#if COMPILE_TORCH
  ++cnt;
#endif
#if COMPILE_BTRCCAR
  ++cnt;
#endif
#if COMPILE_TUNEMAKER
  ++cnt;
#endif	
  return cnt;
}

#endif

void mMainOpen()
{
	buttons_setFuncs(NULL, menu_select, NULL);
	beginAnimation(mOpen);
}

static void mOpen()
{
#if NWATCH_UI_ONLY
    display_setDrawFunc(mMainDraw);
#else
	display_setDrawFunc(menu_draw);
#endif

	buttons_setFuncs(menu_up, menu_select, menu_down);

	setMenuInfo(OPTION_COUNT, MENU_TYPE_ICON, PSTR(STR_MAINMENU));
	
	setMenuFuncs(MENUFUNC_NEXT, mSelect, MENUFUNC_PREV, itemLoader);

	setPrevMenuOpen(&prevMenuData, mOpen);

	beginAnimation2(NULL);
}

static void mSelect()
{
	setPrevMenuExit(&prevMenuData);
	doAction(true);
}

static void itemLoader(byte num)
{
	/* ���пɼ���ڶ�����Ч����������ԭ�˵��ͷ��ض����� */
#if NWATCH_UI_ONLY
    UNUSED(num);
    setMenuOption_P(0, PSTR("Cube Rotation"), menu_tunemaker, tunemakerOpen);
    setMenuOption_P(1, PSTR(STR_DISPLAY), menu_display, mDisplayOpen);
    setMenuOption_P(2, PSTR("CRT Animation"), menu_anim[1], crtDemoOpen);
    setMenuOption_P(3, PSTR("About"), menu_settings, aboutOpen);
    setMenuOption_P(TEMP_MENU_OPTION, PSTR("TEMP / HUM"), tempMenuIcon, tempOpen);
    setMenuOption_P(ENV_MENU_OPTION, PSTR("ENV MONITOR"), envMenuIcon, envOpen);
#else
	num = 0;
	setMenuOption_P(num++, PSTR(STR_ALARMS), menu_alarm, mAlarmsOpen);
#if COMPILE_TORCH
	setMenuOption_P(num++, PSTR(STR_FLASHLIGHT), menu_torch, torch_open);
#endif
#if COMPILE_STOPWATCH
	setMenuOption_P(num++, PSTR(STR_STOPWATCH), menu_stopwatch, stopwatch_open);
#endif
#if COMPILE_BTRCCAR
	setMenuOption_P(num++, PSTR(STR_BTRCCAR), menu_stopwatch, btrccar_open);
#endif
	#if COMPILE_TUNEMAKER
	setMenuOption_P(num++, PSTR(STR_TUNEMAKER), menu_tunemaker, tunemakerOpen);
#endif	
#if COMPILE_GAME1 || COMPILE_GAME2 || COMPILE_GAME3
	setMenuOption_P(num++, PSTR(STR_GAMES), menu_games, mGamesOpen);
#endif
	//setMenuOption_P(num++, PSTR(STR_CALCULATORS), menu_calc, calcOpen);
	setMenuOption_P(num++, PSTR(STR_SETTINGS), menu_settings, mSettingsOpen);
#endif
	setMenuOption_P(OPTION_COUNT, PSTR(STR_EXIT), menu_exit, menu_close);
}

#if NWATCH_UI_ONLY
/* Let the existing carousel advance before presenting the selected entry page.
 * All buttons, selection, horizontal easing and vertical transitions stay owned
 * by the original menu/animation code. Other entries use menu_draw unchanged. */
static display_t mMainDraw(void)
{
    display_t busy = menu_draw();
    if(menuData.selected == TEMP_MENU_OPTION && busy == DISPLAY_DONE)
    {
        byte y;
        /* Replace only this rendered frame; no flush or display-state changes. */
        for(y = 0; y < FRAME_HEIGHT; y += 8)
            draw_clearArea(0, y, FRAME_WIDTH);
        return tempMenuDraw();
    }
    return busy;
}

static display_t tempMenuDraw(void)
{
    draw_string_P(PSTR(STR_MAINMENU), false, 19, 2);
    draw_string("<", false, 15, 28);
    draw_string(">", false, 108, 28);
    draw_bitmap(48, 16, tempMenuIcon, 32, 32, NOINVERT, 0);
    draw_bitmap(44, 12, tempMenuCorners[0], 6, 8, NOINVERT, 0);
    draw_bitmap(78, 12, tempMenuCorners[1], 6, 8, NOINVERT, 0);
    draw_bitmap(44, 44, tempMenuCorners[2], 6, 8, NOINVERT, 0);
    draw_bitmap(78, 44, tempMenuCorners[3], 6, 8, NOINVERT, 0);
    draw_string("TEMP / HUM", false, 30, 55);
    return DISPLAY_DONE;
}

static bool aboutBack(void)
{
    beginAnimation(back);
    return true;
}

static display_t aboutDraw(void)
{
    draw_bitmap(48, 0, menu_settings, 32, 32, NOINVERT, 0);
    draw_string_P(PSTR("NWatch GUI"), false, 29, 32);
    draw_string_P(PSTR("STM32F103 / AC5"), false, 8, 42);
    draw_string_P(PSTR("PLAY: Back"), false, 29, 54);
    return DISPLAY_DONE;
}

/* ��ʪ��ҳ������ԭ�˵��ķ��ض����� */
static bool tempBack(void)
{
    beginAnimation(back);
    return true;
}

/* Page-local primitives use the existing vertical-page 1bpp framebuffer. */
typedef struct
{
    uint8_t *pixels;
    int offsetY;
} temp_canvas_t;

static void tempPixel(const temp_canvas_t *canvas, int x, int y)
{
    if(x < 0 || x >= FRAME_WIDTH || y < 0 || y >= FRAME_HEIGHT)
        return;
    y += canvas->offsetY;
    if(y < 0 || y >= FRAME_HEIGHT)
        return;
    canvas->pixels[x + (y / 8) * FRAME_WIDTH] |= (uint8_t)(1U << (y % 8));
}

static void tempLine(const temp_canvas_t *canvas, int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    int twiceError;

    for(;;)
    {
        tempPixel(canvas, x0, y0);
        if(x0 == x1 && y0 == y1)
            break;
        twiceError = 2 * error;
        if(twiceError >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if(twiceError <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }
}

static void tempRect(const temp_canvas_t *canvas, int x, int y,
                     int width, int height, bool filled)
{
    int row;
    if(width <= 0 || height <= 0)
        return;
    for(row = 0; row < height; ++row)
    {
        if(filled || row == 0 || row == height - 1)
            tempLine(canvas, x, y + row, x + width - 1, y + row);
        else
        {
            tempPixel(canvas, x, y + row);
            tempPixel(canvas, x + width - 1, y + row);
        }
    }
}

static void tempDrawTemperature(const temp_canvas_t *canvas, bool hasData, int temperature)
{
    char number[16];
    char unit[] = {CHAR_DEGREES, 'C', '\0'};
    int length;
    int width;
    int x;
    int i;

    if(hasData)
        snprintf(number, sizeof(number), "%d", temperature);
    else
        strcpy(number, "--");
    length = (int)strlen(number);
    width = length * (MIDFONT_WIDTH + 2) - 2 + 3 + 12;

    /* Keep unexpected, oversized integer values safely inside the display. */
    if(width > FRAME_WIDTH)
    {
        snprintf(number, sizeof(number), "%d%cC", temperature, CHAR_DEGREES);
        x = (FRAME_WIDTH - ((int)strlen(number) * 7 - 2)) / 2;
        draw_string(number, false, (byte)x, 16);
        return;
    }

    x = (FRAME_WIDTH - width) / 2;
    for(i = 0; i < length; ++i)
    {
        if(number[i] >= '0' && number[i] <= '9')
            draw_bitmap((byte)x, 8, midFont[number[i] - '0'],
                        MIDFONT_WIDTH, MIDFONT_HEIGHT, NOINVERT, 0);
        else
            tempRect(canvas, x, 18, MIDFONT_WIDTH, 3, true);
        x += MIDFONT_WIDTH + 2;
    }
    draw_string(unit, false, (byte)(x + 1), 8);
}

static void tempDrawHistory(const temp_canvas_t *canvas, const int *history, uint8_t validCount)
{
    int minimum;
    int maximum;
    int x;
    int y;
    int previousX = 0;
    int previousY = 0;
    uint8_t i;

    if(validCount < 2 || validCount > TEMP_HISTORY_SIZE)
        return;
    minimum = maximum = history[0];
    for(i = 1; i < validCount; ++i)
    {
        if(history[i] < minimum)
            minimum = history[i];
        if(history[i] > maximum)
            maximum = history[i];
    }

    /* History_Get already returns oldest to newest; read only valid entries. */
    for(i = 0; i < validCount; ++i)
    {
        x = (int)i * (FRAME_WIDTH - 1) / (validCount - 1);
        if(maximum == minimum)
            y = 48;
        else
            y = 54 - (history[i] - minimum) * 11 / (maximum - minimum);
        if(i != 0)
            tempLine(canvas, previousX, previousY, x, y);
        previousX = x;
        previousY = y;
    }
}

static display_t tempDraw(void)
{
    sensor_data_t data = {0};
    int tempHistory[TEMP_HISTORY_SIZE];
    uint8_t validCount = 0;
    uint32_t xres, yres, bpp;
    temp_canvas_t canvas;
    char humidity[16];
    bool hasData;
    int hum;
    int offset;

    canvas.pixels = (uint8_t *)LCD_GetFrameBuffer(&xres, &yres, &bpp);
    if(canvas.pixels == NULL || xres != FRAME_WIDTH || yres != FRAME_HEIGHT || bpp != 1)
        return DISPLAY_DONE;
    offset = animation_offsetY();
    canvas.offsetY = offset > 127 ? offset - 256 : offset;
    hasData = g_sensorQueue != NULL && xQueuePeek(g_sensorQueue, &data, 0) == pdPASS;

    draw_string("DHT11", false, 0, 0);
    draw_string(hasData ? "OK" : "WAIT", false, hasData ? 116 : 102, 0);
    tempDrawTemperature(&canvas, hasData, data.temp);

    draw_string("HUM", false, 0, 34);
    tempRect(&canvas, 27, 33, 68, 9, false);
    if(hasData)
    {
        hum = data.hum;
        if(hum < 0)
            hum = 0;
        if(hum > 100)
            hum = 100;
        tempRect(&canvas, 28, 34, (hum * 66 + 50) / 100, 7, true);
        snprintf(humidity, sizeof(humidity), "%d%%", data.hum);
    }
    else
        strcpy(humidity, "--%");
    draw_string(humidity, false, (byte)(FRAME_WIDTH - ((int)strlen(humidity) * 7 - 2)), 34);

    if(History_Get(tempHistory, &validCount) != NULL)
        tempDrawHistory(&canvas, tempHistory, validCount);

    draw_string("KEY: BACK", false, 33, 56);
    return DISPLAY_DONE;
}

static void tempOpen(void)
{
    display_setDrawFunc(tempDraw);
    buttons_setFuncs(tempBack, tempBack, tempBack);
    beginAnimation2(NULL);
}

/* 返回主菜单时沿用原来的退场动画及选中项恢复机制。 */
static bool envBack(void)
{
    beginAnimation(back);
    return true;
}

static display_t envDraw(void)
{
    /* 句柄未就绪时仅在本帧显示错误，不写入事件组。 */
    EventBits_t bits = ENV_SENSOR_ERROR_BIT;
    const char *status[4];
    byte row;

    /* 每帧读取一次快照，不等待、不清位，四行状态保持一致。 */
    if(g_envEventGroup != NULL)
        bits = xEventGroupGetBits(g_envEventGroup);

    status[0] = (bits & ENV_DARK_BIT) ? "DARK" : "LIGHT";
    status[1] = (bits & ENV_OBSTACLE_BIT) ? "DETECTED" : "CLEAR";
    if(bits & ENV_SENSOR_ERROR_BIT)
    {
        status[2] = "ERROR";
        status[3] = "ERROR";
    }
    else
    {
        status[2] = (bits & ENV_NEAR_BIT) ? "NEAR" : "FAR";
        status[3] = "OK";
    }

    draw_string("ENV MONITOR", false, 26, 0);
    for(row = 0; row < 4; ++row)
    {
        /* 四行间距 12 像素，末行止于 y=59；位图自动跟随页面动画。 */
        byte y = 16 + row * 12;
        draw_bitmap(8, y, envStatusIcons[row], 8, 8, NOINVERT, 0);
        draw_string_P(status[row], false, 28, y);
    }
    return DISPLAY_DONE;
}

static void envOpen(void)
{
    display_setDrawFunc(envDraw);
    buttons_setFuncs(envBack, envBack, envBack);
    beginAnimation2(NULL);
}

static void aboutOpen(void)
{
    display_setDrawFunc(aboutDraw);
    buttons_setFuncs(aboutBack, aboutBack, aboutBack);
    beginAnimation2(NULL);
}

/* CRT ��ʾ����ԭ���ֶ������رպ�ʱչ���������ɷ��ء� */
static bool crtDemoBack(void)
{
    crtDemoActive = false;
    display_startCRTAnim(CRTANIM_OPEN);
    beginAnimation(back);
    return true;
}

static display_t crtDemoDraw(void)
{
    aboutDraw();
    return DISPLAY_BUSY;
}

void mMainUpdate(void)
{
    if(crtDemoActive && !crtDemoOpening && (millis_t)(millis() - crtDemoStarted) >= 1000)
    {
        crtDemoOpening = true;
        display_startCRTAnim(CRTANIM_OPEN);
    }
}

static void crtDemoOpen(void)
{
    crtDemoActive = true;
    crtDemoStarted = millis();
    crtDemoOpening = false;
    display_setDrawFunc(crtDemoDraw);
    buttons_setFuncs(crtDemoBack, crtDemoBack, crtDemoBack);
    beginAnimation2(NULL);
    display_startCRTAnim(CRTANIM_CLOSE);
}
#endif
