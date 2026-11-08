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

#define OPTION_COUNT 5
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
	display_setDrawFunc(menu_draw);

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
	/* 所有可见入口都有有效动作，沿用原菜单和返回动画。 */
#if NWATCH_UI_ONLY
    UNUSED(num);
    setMenuOption_P(0, PSTR("Cube Rotation"), menu_tunemaker, tunemakerOpen);
    setMenuOption_P(1, PSTR(STR_DISPLAY), menu_display, mDisplayOpen);
    setMenuOption_P(2, PSTR("CRT Animation"), menu_anim[1], crtDemoOpen);
    setMenuOption_P(3, PSTR("About"), menu_settings, aboutOpen);
    setMenuOption_P(4, PSTR("Temp / Hum"), menu_default, tempOpen);
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

/* 温湿度页面沿用原菜单的返回动画。 */
static bool tempBack(void)
{
    beginAnimation(back);
    return true;
}

/* 非阻塞查看最新队列数据，不取走消息，不直接操作 OLED。 */
static display_t tempDraw(void)
{
    sensor_data_t data;
    char line[24];

    draw_string("TEMP / HUMIDITY", false, 11, 0);
    if(g_sensorQueue != NULL &&
       xQueuePeek(g_sensorQueue, &data, 0) == pdPASS)
    {
        /* 使用固定大小的栈缓冲区构造文字，不申请动态内存。 */
        snprintf(line, sizeof(line), "TEMP: %d C", data.temp);
        draw_string(line, false, 10, 20);
        snprintf(line, sizeof(line), "HUM : %d %%", data.hum);
        draw_string(line, false, 10, 36);
    }
    else
        draw_string("Waiting DHT11", false, 15, 28);

    draw_string("KEY: Back", false, 32, 56);
    /* 由原显示框架持续刷新，进入和返回时仍使用原有动画。 */
    return DISPLAY_DONE;
}

static void tempOpen(void)
{
    display_setDrawFunc(tempDraw);
    buttons_setFuncs(tempBack, tempBack, tempBack);
    beginAnimation2(NULL);
}

static void aboutOpen(void)
{
    display_setDrawFunc(aboutDraw);
    buttons_setFuncs(aboutBack, aboutBack, aboutBack);
    beginAnimation2(NULL);
}

/* CRT 演示沿用原遮罩动画，关闭后定时展开，按键可返回。 */
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
