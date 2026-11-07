from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
assert root.name == 'NWatch-DShanMCU-F103-RTOS'
p = root/'NWatch/m_main.c'
s = p.read_bytes().decode('latin1').replace('\r\n','\n')
assert '#define OPTION_COUNT 3' in s
s = s.replace('#define OPTION_COUNT 3', '#define OPTION_COUNT 4', 1)
s = s.replace('static bool aboutBack(void);', '''static bool aboutBack(void);
static void crtDemoOpen(void);
static display_t crtDemoDraw(void);
static bool crtDemoBack(void);
static millis_t crtDemoStarted;
static bool crtDemoOpening;''', 1)
s = s.replace('    setMenuOption_P(2, PSTR("About"), menu_settings, aboutOpen);', '''    setMenuOption_P(2, PSTR("CRT Animation"), menu_anim[1], crtDemoOpen);
    setMenuOption_P(3, PSTR("About"), menu_settings, aboutOpen);''', 1)
pos = s.rfind('#endif')
s = s[:pos] + '''
/* CRT ''' + '演示沿用原遮罩动画，关闭后定时展开，按键可返回。'.encode('gbk').decode('latin1') + ''' */
static bool crtDemoBack(void)
{
    display_startCRTAnim(CRTANIM_OPEN);
    beginAnimation(back);
    return true;
}

static display_t crtDemoDraw(void)
{
    aboutDraw();
    if(!crtDemoOpening && (millis_t)(millis() - crtDemoStarted) >= 1000)
    {
        crtDemoOpening = true;
        display_startCRTAnim(CRTANIM_OPEN);
    }
    return DISPLAY_BUSY;
}

static void crtDemoOpen(void)
{
    crtDemoStarted = millis();
    crtDemoOpening = false;
    display_setDrawFunc(crtDemoDraw);
    buttons_setFuncs(crtDemoBack, crtDemoBack, crtDemoBack);
    beginAnimation2(NULL);
    display_startCRTAnim(CRTANIM_CLOSE);
}
''' + s[pos:]
# display_update stops calling drawFunc after CRT closes. Schedule reopen outside
# the draw callback, so the demo cannot leave the OLED closed indefinitely.
s = s.replace('static bool crtDemoOpening;', 'static bool crtDemoOpening;\nstatic bool crtDemoActive;')
s = s.replace('    display_startCRTAnim(CRTANIM_OPEN);\n    beginAnimation(back);', '    crtDemoActive = false;\n    display_startCRTAnim(CRTANIM_OPEN);\n    beginAnimation(back);')
s = s.replace('    if(!crtDemoOpening && (millis_t)(millis() - crtDemoStarted) >= 1000)', '    return DISPLAY_BUSY;\n}\n\nvoid mMainUpdate(void)\n{\n    if(crtDemoActive && !crtDemoOpening && (millis_t)(millis() - crtDemoStarted) >= 1000)')
s = s.replace('        display_startCRTAnim(CRTANIM_OPEN);\n    }\n    return DISPLAY_BUSY;\n}', '        display_startCRTAnim(CRTANIM_OPEN);\n    }\n}')
s = s.replace('    crtDemoStarted = millis();', '    crtDemoActive = true;\n    crtDemoStarted = millis();')
p.write_bytes(s.replace('\n','\r\n').encode('latin1'))

p = root/'NWatch/m_main.h'
raw = p.read_bytes()
backup = root/'MDK-ARM/phase1/before/NWatch/m_main.h'
backup.write_bytes(raw)
s = raw.decode('latin1').replace('void mMainOpen(void);', 'void mMainOpen(void);\n#if NWATCH_UI_ONLY\nvoid mMainUpdate(void);\n#endif')
p.write_bytes(s.encode('latin1'))

p = root/'Core/Src/main.c'
s = p.read_bytes().decode('latin1').replace('\r\n','\n')
s = s.replace('\t\t\t display_update();', '#if NWATCH_UI_ONLY\n\t\t\t mMainUpdate();\n#endif\n\t\t\t display_update();',1)
p.write_bytes(s.replace('\n','\r\n').encode('latin1'))

# Keep rapid repeated PLAY events from interrupting the callback that opens a page.
p = root/'NWatch/menu.c'
raw = p.read_bytes()
(root/'MDK-ARM/phase1/before/NWatch/menu.c').write_bytes(raw)
s = raw.decode('latin1').replace('\r\n','\n')
s = s.replace('if(!animation_active() || animation_movingOn())', 'if(!animation_active())', 1)
s = s.replace('(!animation_active() || animation_movingOn())', '!animation_active()', 1)
p.write_bytes(s.replace('\n','\r\n').encode('latin1'))
