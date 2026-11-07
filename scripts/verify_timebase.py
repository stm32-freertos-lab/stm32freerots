"""Inspect the actual AC5 build. This does not simulate STM32 hardware."""
from pathlib import Path
import difflib
import hashlib
import json
import re
import xml.etree.ElementTree as ET

mdk = Path(__file__).resolve().parent.parent
root = mdk.parent
stage = mdk/'phase2_timebase'
out = mdk/'build/NWatch-DShanMCU-F103'
params = json.loads((out/'builder.params').read_text())
map_text = (out/'NWatch-DShanMCU-F103.map').read_text()
assert params['toolchain'] == 'AC5' and params['rom'] == 65536 and params['ram'] == 20480
assert params['options']['c/cpp-compiler']['optimization'] == 'level-3'
sources = set(params['sourceList'])
assert len(sources) == 42
assert not any(re.search(r'FreeRTOS|portable/|heap_[1-5]\.c|tasks\.c',p,re.I) for p in sources)
assert '../Core/Src/tim.c' not in sources  # old generated TIM3 PWM must stay excluded

uv = ET.parse(mdk/'NWatch-DShanMCU-F103.uvprojx')
active_uv = {f.findtext('FilePath').replace('\\','/')
             for f in uv.findall('.//Groups/Group/Files/File')
             if f.findtext('FileOption/CommonProperty/IncludeInBuild') != '0'}
active_uv = {p[2:] if p.startswith('./') else p for p in active_uv}
assert sources == active_uv

for symbol,obj in [('HAL_InitTick','driver_systick.o'),
                   ('SysTick_Handler','stm32f1xx_it.o'),
                   ('TIM3_IRQHandler','stm32f1xx_it.o'),
                   ('TIM4_IRQHandler','stm32f1xx_it.o'),
                   ('system_get_us','driver_timer.o')]:
    assert re.search(r'^\s+'+symbol+r'\s+0x[\da-f]+\s+Thumb Code\s+\d+\s+'+re.escape(obj),map_text,re.M),symbol
assert re.search(r'^\s+SysTick_Handler\s+0x[\da-f]+\s+Thumb Code\s+2\s+',map_text,re.M)
assert not re.search(r'^\s+HAL_SYSTICK_Config\s+0x[\da-f]+\s+Thumb Code',map_text,re.M)

# Active application sources must not touch the SysTick registers/configuration.
for source in sources:
    if '/STM32F1xx_HAL_Driver/' in source:
        continue  # unchanged vendor weak implementations are removed by the linker
    text=(mdk/source).read_bytes().decode('latin1')
    assert not re.search(r'SysTick\s*->|HAL_SYSTICK_Config\s*\(|SysTick_Config\s*\(',text),source

it=(root/'Core/Src/stm32f1xx_it.c').read_text(encoding='utf-8')
assert 'timeDate.' not in it and 'HAL_IncTick' not in it
assert 'systick_t' not in it
ir=(root/'Drivers/DShanMCU-F103/driver_ir_receiver.c').read_text(encoding='utf-8')
assert 'system_get_ns()' not in ir
assert 'static uint32_t g_IRReceiverIRQ_Timers[68]' in ir
assert 'now - pre_time > 100000ULL' in ir
assert 'time < 3500U || time > 5500U' in ir

def metrics(p):
    s=p.read_text()
    return {key:int(re.search(r'Total '+label+r'\s*\([^\n]+?\)\s+(\d+)',s)[1])
            for key,label in [('ro','RO  Size'),('ram','RW  Size'),('flash','ROM Size')]}

def flash_span(p):
    base=0
    lo=2**32
    hi=0
    count=0
    for line in p.read_text().splitlines():
        b=bytes.fromhex(line[1:])
        assert sum(b)%256 == 0
        n,off,kind=b[0],int.from_bytes(b[1:3],'big'),b[3]
        if kind==4:
            base=int.from_bytes(b[4:4+n],'big')<<16
        elif kind==0:
            lo=min(lo,base+off)
            hi=max(hi,base+off+n-1)
            count+=n
    assert lo==0x08000000
    return count,hi-lo+1

old=metrics(stage/'baseline.map')
new=metrics(out/'NWatch-DShanMCU-F103.map')
assert flash_span(out/'NWatch-DShanMCU-F103.hex')==(new['flash'],new['flash'])
assert new['flash']<=65536 and new['ram']<=20480

# UI resources that were backed up in phase 1 must match the verified phase-1 build.
phase1=json.loads((mdk/'phase1/changed-sources.json').read_text())
for rel in ['NWatch/resources.c','NWatch/watchface.c','NWatch/watchface.h',
            'NWatch/tunemaker.c','NWatch/m_main.c','NWatch/m_main.h',
            'NWatch/menu.c','NWatch/m_display.c','NWatch/normal.c']:
    assert hashlib.sha256((root/rel).read_bytes()).hexdigest()==phase1[rel]['after_sha256'],rel

manifest={}
diff=[]
for p in sorted((stage/'before').rglob('*')):
    if not p.is_file():continue
    rel=p.relative_to(stage/'before')
    a=p.read_bytes(); b=(root/rel).read_bytes()
    manifest[str(rel).replace('\\','/')]={'before_sha256':hashlib.sha256(a).hexdigest(),
                                       'after_sha256':hashlib.sha256(b).hexdigest()}
    diff.extend(difflib.unified_diff(a.decode('latin1').splitlines(True),
                b.decode('latin1').splitlines(True),fromfile='before/'+str(rel),tofile='after/'+str(rel)))
(stage/'source-changes.diff').write_bytes(''.join(diff).encode('latin1'))
(stage/'changed-sources.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
result={'compiler':'ARM Compiler 5.06 update 5 (build 528)', 'baseline':old,'timebase':new,
        'delta_flash':new['flash']-old['flash'],'delta_ram':new['ram']-old['ram'],
        'warnings':len(re.findall('Warning:',(out/'compiler.log').read_text())),
        'sources':len(sources), 'changed_files':list(manifest),
        'checks':['EIDE/uVision sources agree','AC5/O3/64KB/20KB unchanged',
                  'HAL_InitTick binds to TIM4 implementation','empty SysTick handler',
                  'no SysTick access in active application sources',
                  'no FreeRTOS kernel/task/heap sources',
                  'original UI source hashes unchanged','HEX checksum/address span'],
        'hardware_tested':False}
(stage/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
