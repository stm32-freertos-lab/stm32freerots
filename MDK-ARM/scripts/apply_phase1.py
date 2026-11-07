"""One-time phase 1 edits; preserve original source bytes and a local backup."""
from pathlib import Path
import re
import hashlib
import json

mdk = Path(__file__).resolve().parent.parent
root = mdk.parent
assert root.name == 'NWatch-DShanMCU-F103-RTOS'
backup = mdk / 'phase1' / 'before'
changes = {}

def read(rel):
    p = root / rel
    raw = p.read_bytes()
    changes[rel] = raw
    return raw.decode('latin1').replace('\r\n', '\n')

def cn(s):
    return s.encode('gbk').decode('latin1')

def put(rel, text):
    p = root / rel
    b = backup / rel
    b.parent.mkdir(parents=True, exist_ok=True)
    assert not b.exists(), f'Already backed up: {rel}'
    b.write_bytes(changes[rel])
    p.write_bytes(text.replace('\n', '\r\n').encode('latin1'))

def replace(s, old, new):
    assert old in s, old[:100]
    return s.replace(old, new, 1)

s = read('NWatch/config.h')
start = s.index('#define COMPILE_GAME1')
end = s.index('\n\n', s.index('#define COMPILE_TUNEMAKER'))
s = s[:start] + cn('/* 第一阶段：保留原 UI 和动画，关闭手表业务。 */\n') + '''#ifndef NWATCH_UI_ONLY
#define NWATCH_UI_ONLY 1
#endif

#if NWATCH_UI_ONLY
#define COMPILE_GAME1      0
#define COMPILE_GAME2      0
#define COMPILE_GAME3      0
#define COMPILE_ANIMATIONS 1
#define COMPILE_STOPWATCH  0
#define COMPILE_TORCH      0
#define COMPILE_TUNEMAKER  1
#else
''' + s[start:end] + '\n#endif' + s[end:]
put('NWatch/config.h', s)

s = read('Core/Src/main.c')
s = replace(s, 'SPI_HandleTypeDef hspi1;', '#if !NWATCH_UI_ONLY\nSPI_HandleTypeDef hspi1;')
s = replace(s, 'UART_HandleTypeDef huart1;', 'UART_HandleTypeDef huart1;\n#endif')
s = replace(s, 'static void MX_TIM1_Init(void);', '#if !NWATCH_UI_ONLY\nstatic void MX_TIM1_Init(void);')
s = replace(s, 'static void MX_SPI1_Init(void);', 'static void MX_SPI1_Init(void);\n#endif')
s = replace(s, '  MX_TIM1_Init();', '#if !NWATCH_UI_ONLY\n  MX_TIM1_Init();')
s = replace(s, '  MX_SPI1_Init();', '  MX_SPI1_Init();\n#endif')
s = replace(s, '\tPassiveBuzzer_Init();', '#if !NWATCH_UI_ONLY\n\tPassiveBuzzer_Init();')
s = replace(s, '\tDHT11_Init();', '\tDHT11_Init();\n#endif')
a = s.index('\tconsole_log(50,"Init_OLED: OK");')
b = s.index('\n\ttime_init();', a)
s = s[:a] + '#if !NWATCH_UI_ONLY\n' + s[a:b] + '\n#endif\n' + s[b:]
a = s.index('\talarm_init();')
b = s.index('\n  buttons_init();', a)
s = s[:a] + '#if !NWATCH_UI_ONLY\n' + s[a:b] + '\n#endif' + s[b:]
a = s.index('\tconsole_log(50, "Init_MPU: OK!");')
b = s.index('\n\tLCD_Clear();', a)
s = s[:a] + '#if !NWATCH_UI_ONLY\n' + s[a:b] + '\n#endif\n' + s[b:]
s = replace(s, '\t\tbuzzer_update();', '#if !NWATCH_UI_ONLY\n\t\tbuzzer_update();')
s = replace(s, '\t\tled_update();', '\t\tled_update();\n#endif')
s = replace(s, '\t\t\t alarm_update();', '#if !NWATCH_UI_ONLY\n\t\t\t alarm_update();\n#endif')
for func in ['MX_SPI1_Init', 'MX_TIM1_Init', 'MX_TIM2_Init', 'MX_USART1_UART_Init']:
    pat = r'(static void ' + func + r'\(void\)\n\{.*?\n\})'
    s, count = re.subn(pat, r'#if !NWATCH_UI_ONLY\n\1\n#endif', s, flags=re.S)
    assert count == 1
put('Core/Src/main.c', s)

s = read('NWatch/appconfig.c')
s = replace(s, '\tuint8_t try_count = 3;', cn('\t/* 精简版使用 RAM 默认配置，不再依赖外部 Flash。 */\n') + '#if NWATCH_UI_ONLY\n\tappconfig_reset();\n#else\n\tuint8_t try_count = 3;')
s = replace(s, '\tappconfig_reset();\n}', '\tappconfig_reset();\n#endif\n}')
s = replace(s, '\tW25Q64_Erase(', '#if !NWATCH_UI_ONLY\n\tW25Q64_Erase(')
s = replace(s, '\tW25Q64_Write(FLASH_SECTOR_APP_CONFIG, (uint8_t *)&appConfig, sizeof(appconfig_s));', '\tW25Q64_Write(FLASH_SECTOR_APP_CONFIG, (uint8_t *)&appConfig, sizeof(appconfig_s));\n#endif')
s = replace(s, '\tappConfig.sleepTimeout = 1;', cn('\t/* 保持 GUI 在线，避免原低功耗流程在无 RTC 时无法唤醒。 */\n') + '\tappConfig.sleepTimeout = NWATCH_UI_ONLY ? 0 : 1;')
s = replace(s, '\talarm_reset();', '#if !NWATCH_UI_ONLY\n\talarm_reset();\n#endif')
put('NWatch/appconfig.c', s)

s = read('NWatch/buttons.c')
s = replace(s, '  const tune_t* tune;', '#if !NWATCH_UI_ONLY\n  const tune_t* tune;')
s = replace(s, '} s_button;', '#endif\n} s_button;')
s = replace(s, '  buttons[BTN_1].tune', '#if !NWATCH_UI_ONLY\n  buttons[BTN_1].tune')
s = replace(s, '  buttons[BTN_3].tune = tuneBtn3;', '  buttons[BTN_3].tune = tuneBtn3;\n#endif')
s = replace(s, '        tune_play(button->tune', '#if !NWATCH_UI_ONLY\n        tune_play(button->tune')
s = replace(s, '        led_flash(LED_GREEN, LED_FLASH_FAST, LED_BRIGHTNESS_MAX);', '        led_flash(LED_GREEN, LED_FLASH_FAST, LED_BRIGHTNESS_MAX);\n#endif')
put('NWatch/buttons.c', s)

s = read('NWatch/global.c')
s = replace(s, '\tstatic BOOL lastUsbConnected;', '#if !NWATCH_UI_ONLY\n\tstatic BOOL lastUsbConnected;')
s = replace(s, '\n}\n\n// Dividing', '\n#endif\n}\n\n// Dividing')
put('NWatch/global.c', s)

s = read('NWatch/normal.c')
s = replace(s, '\tif(!appConfig.showFPS)', '#if !NWATCH_UI_ONLY\n\tif(!appConfig.showFPS)')
s = replace(s, '\n\n\treturn busy;', '\n#endif\n\n\treturn busy;')
put('NWatch/normal.c', s)

s = read('NWatch/time.c')
s = replace(s, '\talarm_updateNextAlarm();', '#if !NWATCH_UI_ONLY\n\talarm_updateNextAlarm();\n#endif')
s = replace(s, '\tif(timeDate.time.secs == 0 && timeDate.time.mins == 0)\n\t\ttune_play(tuneHour, VOL_HOUR, PRIO_HOUR);', '#if !NWATCH_UI_ONLY\n\tif(timeDate.time.secs == 0 && timeDate.time.mins == 0)\n\t\ttune_play(tuneHour, VOL_HOUR, PRIO_HOUR);\n#endif')
s = replace(s, '\t\t\ttune_play(tuneHour, VOL_HOUR, PRIO_HOUR);', '#if !NWATCH_UI_ONLY\n\t\t\ttune_play(tuneHour, VOL_HOUR, PRIO_HOUR);\n#endif')
put('NWatch/time.c', s)

s = read('NWatch/pwrmgr.c')
s = replace(s, '\t\tled_flash(LED_BLUE, LED_FLASH_FAST, LED_BRIGHTNESS_MAX);', '#if !NWATCH_UI_ONLY\n\t\tled_flash(LED_BLUE, LED_FLASH_FAST, LED_BRIGHTNESS_MAX);\n#endif')
put('NWatch/pwrmgr.c', s)

s = read('NWatch/watchface.c')
s = replace(s, 'uint8_t vertices[1440][3] =', cn('/* 两个立方体页面共享只读顶点表，放入片内 Flash。 */\n') + 'const uint8_t cubeVertices[1440][3] =')
s = re.sub(r'\bvertices\[', 'cubeVertices[', s)
put('NWatch/watchface.c', s)
s = read('NWatch/watchface.h')
s = replace(s, 'void watchfaceOpen(void);', '#include <stdint.h>\n\nextern const uint8_t cubeVertices[1440][3];\n\nvoid watchfaceOpen(void);')
put('NWatch/watchface.h', s)

s = read('NWatch/tunemaker.c')
s, count = re.subn(r'static const uint8_t vertices\[1440\]\[3\]\s*=\s*\{.*?\};', cn('/* 复用 watchface.c 的 Flash 顶点数据，保留原旋转动画。 */'), s, count=1, flags=re.S)
assert count == 1
s = re.sub(r'\bvertices\[', 'cubeVertices[', s)
s = replace(s, 'extern const tune_t TUNE[];', '#if !NWATCH_UI_ONLY\nextern const tune_t TUNE[];')
s = replace(s, '\nvoid tunemakerOpen()', '\n#endif\n\nvoid tunemakerOpen()')
s = replace(s, '\tchose_tone(1);', '#if !NWATCH_UI_ONLY\n\tchose_tone(1);\n#endif')
s = replace(s, '\tidx--;', '#if !NWATCH_UI_ONLY\n\tidx--;')
s = replace(s, '\tif(idx == 255) idx= 1;', '\tif(idx == 255) idx= 1;\n#endif')
s = replace(s, '\tidx++;', '#if !NWATCH_UI_ONLY\n\tidx++;')
s = replace(s, '\tif(idx == IDX_MAX) idx= 0;', '\tif(idx == IDX_MAX) idx= 0;\n#endif')
s = replace(s, '\tif(iAng == 0) iAng= 180;', '\tif(iAng >= 180) iAng= 179;')
s = replace(s, '\ttune_stop(PRIO_HIGH);', '#if !NWATCH_UI_ONLY\n\ttune_stop(PRIO_HIGH);\n#endif')
s = replace(s, '\t\t\tchose_tone(0);', '#if !NWATCH_UI_ONLY\n\t\t\tchose_tone(0);')
s = replace(s, '\t    ///LED0=!LED0;', '#endif\n\t    ///LED0=!LED0;')
s = replace(s, '\t\tif(iAng == 180) iAng = 0;', '\t\tif(iAng >= 180) iAng = 0;')
put('NWatch/tunemaker.c', s)

s = read('NWatch/m_display.c')
s = replace(s, '#define OPTION_COUNT\t5', '#if NWATCH_UI_ONLY\n#define OPTION_COUNT 4\n#else\n#define OPTION_COUNT 5\n#endif')
a = s.index('static void setBrightness()\n')
b = s.index('\nstatic void setInvert()', a)
s = s[:a] + cn('/* 与亮度图标的四个索引一致，避免越界。 */\n') + '''static void setBrightness()
{
    static const byte levels[] = {0x01, 0x0f, 0x5f, 0xff};
    appConfig.brightness = (appConfig.brightness + 1) % 4;
    LCD_SetDisplayBacklight(levels[appConfig.brightness]);
}
''' + s[b:]
s = replace(s, 'static void setLEDs()', '#if !NWATCH_UI_ONLY\nstatic void setLEDs()')
s = replace(s, '\n#if COMPILE_ANIMATIONS\nstatic void setAnimations()', '\n#endif\n\n#if COMPILE_ANIMATIONS\nstatic void setAnimations()')
s = replace(s, '\tsetMenuOption_P(4, PSTR(STR_LEDS)', '#if !NWATCH_UI_ONLY\n\tsetMenuOption_P(4, PSTR(STR_LEDS)')
s = replace(s, 'menu_LEDs[appConfig.CTRL_LEDs], setLEDs);', 'menu_LEDs[appConfig.CTRL_LEDs], setLEDs);\n#endif')
put('NWatch/m_display.c', s)

s = read('NWatch/m_main.c')
s = replace(s, '\n#define OPTION_COUNT getItemCount()', '''
#if NWATCH_UI_ONLY
#define OPTION_COUNT 3
static void aboutOpen(void);
static display_t aboutDraw(void);
static bool aboutBack(void);
#else
#define OPTION_COUNT getItemCount()
#endif''')
s = replace(s, 'static uint8_t getItemCount()', '#if !NWATCH_UI_ONLY\nstatic uint8_t getItemCount()')
s = replace(s, '\nvoid mMainOpen()', '\n#endif\n\nvoid mMainOpen()')
s = replace(s, '\tnum = 0;\n', cn('\t/* 所有可见入口都有有效动作，沿用原菜单和返回动画。 */\n') + '''#if NWATCH_UI_ONLY
    UNUSED(num);
    setMenuOption_P(0, PSTR("Cube Rotation"), menu_tunemaker, tunemakerOpen);
    setMenuOption_P(1, PSTR(STR_DISPLAY), menu_display, mDisplayOpen);
    setMenuOption_P(2, PSTR("About"), menu_settings, aboutOpen);
#else
\tnum = 0;
''')
s = replace(s, '\tsetMenuOption_P(OPTION_COUNT,', '#endif\n\tsetMenuOption_P(OPTION_COUNT,')
s += '''
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

static void aboutOpen(void)
{
    display_setDrawFunc(aboutDraw);
    buttons_setFuncs(aboutBack, aboutBack, aboutBack);
    beginAnimation2(NULL);
}
#endif
'''
put('NWatch/m_main.c', s)

# ARMCC keeps resource arrays in one RO section: exclude unused application art.
s = read('NWatch/resources.c')
disabled = {'livesImg','stopwatch','dowImg','menu_timedate','menu_sleep',
            'menu_sound','menu_games','menu_LEDs','menu_volume',
            'menu_sleeptimeout','menu_alarm','menu_stopwatch','menu_torch',
            'menu_diagnostic'}
for name in sorted(disabled):
    pat = r'(const byte ' + name + r'\[.*?\};)'
    s, count = re.subn(pat, r'#if !NWATCH_UI_ONLY\n\1\n#endif', s, count=1, flags=re.S)
    assert count == 1, name
put('NWatch/resources.c', s)

(mdk/'phase1'/'changed-sources.json').write_text(json.dumps({r:hashlib.sha256(b).hexdigest() for r,b in changes.items()},indent=2),encoding='utf-8')
print('Updated:', '\n'.join(changes))
