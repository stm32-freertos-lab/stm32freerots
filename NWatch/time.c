/*
 * Project: N|Watch
 * Author: Zak Kemble, contact@zakkemble.co.uk
 * Copyright: (C) 2013 by Zak Kemble
 * License: GNU GPL v3 (see License.txt)
 * Web: http://blog.zakkemble.co.uk/diy-digital-wristwatch/
 */

#include "common.h"

#define SECONDS_IN_MIN	60
#define SECONDS_IN_HOUR	(60 * SECONDS_IN_MIN)
#define SECONDS_IN_DAY	(((uint32_t)24) * SECONDS_IN_HOUR)

#define FEB_LEAP_YEAR	29
static const byte monthDayCount[] PROGMEM = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

timeDate_s timeDate;
//static timestamp_t timestamp;
/* 软件表盘只在主循环更新，不再由中断直接修改 timeDate。 */
static millis_t lastSecondMs;
static bool clockRunning;

#ifdef RTC_SRC
static void getRtcTime(void);
#endif

void time_init()
{
	timeDate.date.year 	= '0'-25;
	timeDate.date.month = (month_t)9;
	timeDate.date.day		= DAY_TUE;
	timeDate.date.date 	= 12;
	timeDate.date.day = time_dow(timeDate.date.year, timeDate.date.month, timeDate.date.date);
	timeDate.time.ampm = CHAR_24;
	time_timeMode(&timeDate.time, appConfig.timeMode);
	
	time_wake();
}

void time_sleep()
{
    /* 普通休眠不停止软件钟；唤醒后的主循环按已过毫秒补计。 */
    time_update();
}

void time_shutdown()
{
    time_update();
    clockRunning = false;
}


rtcwake_t time_wake()
{
#ifdef RTC_SRC
    getRtcTime();
#endif
    if(!clockRunning)
        lastSecondMs = millis_get();
    clockRunning = true;
    return RTCWAKE_SYSTEM;
}



void time_set(timeDate_s* newTimeDate)
{

	memcpy(&timeDate, newTimeDate, sizeof(timeDate_s));

	timeDate.time.secs = 0;
	lastSecondMs = millis_get();
	time_timeMode(&timeDate.time, appConfig.timeMode);

	///rtc_save(&timeDate);
//	getRtcTime();

#if !NWATCH_UI_ONLY
	alarm_updateNextAlarm();
#endif
}

bool time_isLeapYear(byte year)
{
// Watch only supports years 2000 - 2099, so no need to do the full calculation

	return (year % 4 == 0);

	//uint fullYear = year + 2000;
	//return ((fullYear & 3) == 0 && ((fullYear % 25) != 0 || (fullYear & 15) == 0));
}

// Workout day of week from year, month and date
// http://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
day_t time_dow(byte yy, month_t m, byte d)
{
	//static byte t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	//y -= m < 3;
	//byte dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;

  m =(month_t)(m + (month_t)1);
	int y = yy + 2000;
	byte dow = (d+=m<3?y--:y-2,23*m/9+d+4+y/4-y/100+y/400)%7 ;

	// 0 = Sunday, 1 = Monday, but Monday should be 0
	dow--;
	if(dow == 255) // Overflowed, must have been 0 which is Sunday
		dow = 6;
	return (day_t)dow;
}

byte time_monthDayCount(month_t month, byte year)
{
//	byte numDays = pgm_read_byte(&monthDayCount[month]);
		byte numDays = monthDayCount[month];

	if(month == MONTH_FEB && time_isLeapYear(year))
		numDays = FEB_LEAP_YEAR;
	return numDays;
}


char* time_timeStr()
{
	static char buff[BUFFSIZE_TIME_FORMAT_SMALL];
	sprintf_P(buff, PSTR(TIME_FORMAT_SMALL), timeDate.time.hour, timeDate.time.mins, timeDate.time.ampm);
	return buff;
}

void time_timeMode(time_s* time, timemode_t mode)
{
	byte hour = time->hour;
	if(mode == TIMEMODE_12HR)
	{
		if(time->ampm != CHAR_24) // Already 12hr
			return;
		else if(hour >= 12)
		{
			if(hour > 12)
				hour -= 12;
			time->ampm = CHAR_PM;
		}
		else
		{
			if(hour == 0)
				hour = 12;
			time->ampm = CHAR_AM;
		}
	}
	else // 24 hour
	{
		if(time->ampm == CHAR_AM && hour == 12) // Midnight 12AM = 00:00
			hour = 0;
		else if(time->ampm == CHAR_PM && hour < 12) // No change for 12PM (midday)
			hour += 12;
		time->ampm = CHAR_24;
	}
	
	time->hour = hour;
}

void time_update()
{
    millis_t now = millis_get();
    uint32_t elapsed = (millis_t)(now - lastSecondMs) / 1000U;
    uint32_t total;
    uint32_t daysElapsed;
    time_s time;

    if(!clockRunning || elapsed == 0U)
        return;
    /* 无符号差值跨 32 位毫秒回绕；保留不足一秒部分，不丢失延迟更新。 */
    lastSecondMs += elapsed * 1000U;
#ifdef RTC_SRC
    getRtcTime();
#else
    time = timeDate.time;
    time_timeMode(&time, TIMEMODE_24HR);
    total = (uint32_t)time.hour * SECONDS_IN_HOUR +
            (uint32_t)time.mins * SECONDS_IN_MIN + time.secs + elapsed;
    daysElapsed = total / SECONDS_IN_DAY;
    total %= SECONDS_IN_DAY;
    time.hour = total / SECONDS_IN_HOUR;
    time.mins = (total % SECONDS_IN_HOUR) / SECONDS_IN_MIN;
    time.secs = total % SECONDS_IN_MIN;
    time_timeMode(&time, appConfig.timeMode);
    timeDate.time = time;

    /* 月份数组为 0..11，日期进位与闰年使用同一编号约定。 */
    while(daysElapsed-- != 0U)
    {
        if(++timeDate.date.date > time_monthDayCount(timeDate.date.month, timeDate.date.year))
        {
            timeDate.date.date = 1;
            if(++timeDate.date.month >= 12)
            {
                timeDate.date.month = MONTH_JAN;
                if(++timeDate.date.year == 100)
                    timeDate.date.year = 0;
            }
        }
        timeDate.date.day = (day_t)(((byte)timeDate.date.day + 1U) % 7U);
    }
#endif
#if !NWATCH_UI_ONLY
    if(timeDate.time.secs == 0 && timeDate.time.mins == 0)
        tune_play(tuneHour, VOL_HOUR, PRIO_HOUR);
#endif
}

#ifdef RTC_SRC
static void getRtcTime()
{
    rtc_get(&timeDate);
    timeDate.time.ampm = CHAR_24;
    time_timeMode(&timeDate.time, appConfig.timeMode);
}
#endif
