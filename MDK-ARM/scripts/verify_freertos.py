"""Verify the actual AC5 image and preserve honest limits on runtime claims."""
from pathlib import Path
import hashlib
import json
import re
import struct
import zipfile
import xml.etree.ElementTree as ET

mdk = Path(__file__).resolve().parent.parent
root = mdk.parent
stage = mdk/'phase3_freertos'
out = mdk/'build/NWatch-DShanMCU-F103'
baseline=json.loads((stage/'baseline/manifest.json').read_text())
params=json.loads((out/'builder.params').read_text())
with zipfile.ZipFile(stage/'baseline/timebase-project.zip') as z:
    for rel,sha in baseline.items():
        assert hashlib.sha256(z.read(rel)).hexdigest()==sha,rel
    base_params=json.loads(z.read('MDK-ARM/build/NWatch-DShanMCU-F103/builder.params'))

expected_changes={'Core/Src/main.c','Core/Src/stm32f1xx_it.c',
                  'MDK-ARM/.eide/eide.yml','MDK-ARM/NWatch-DShanMCU-F103.uvprojx'}
source_changes=[]
for rel,sha in baseline.items():
    p=root/rel
    if p.suffix.lower() in {'.c','.h','.s','.uvprojx','.yml','.cjs','.ps1','.py'}:
        if hashlib.sha256(p.read_bytes()).hexdigest()!=sha:
            source_changes.append(rel)
assert set(source_changes)==expected_changes,source_changes

origin=json.loads((stage/'source-origin.json').read_text())
archive=stage/'FreeRTOS-KernelV11.3.1.zip'
assert hashlib.sha256(archive.read_bytes()).hexdigest()==origin['archive_sha256']
vendor_count=0
with zipfile.ZipFile(archive) as z:
    for i in z.infolist():
        if i.is_dir():continue
        rel=i.filename.split('/',1)[1]
        assert (mdk/'ThirdParty/FreeRTOS-Kernel'/rel).read_bytes()==z.read(i),rel
        vendor_count+=1

assert params['toolchain']=='AC5' and params['rom']==65536 and params['ram']==20480
for key in ['global','c/cpp-compiler','asm-compiler']:
    assert params['options'][key]==base_params['options'][key],key
assert params['defines']==base_params['defines']  # bring-up-only flag must be removed
assert len(params['sourceList'])==48 and len(set(params['sourceList']))==48
kernel={'ThirdParty/FreeRTOS-Kernel/'+p for p in ['tasks.c','queue.c','list.c',
        'portable/RVDS/ARM_CM3/port.c','portable/MemMang/heap_4.c']}
assert {p for p in params['sourceList'] if p.startswith('ThirdParty/')}==kernel
assert set(base_params['sourceList'])|kernel|{'RTOS/rtos_app.c'}==set(params['sourceList'])
uv=ET.parse(mdk/'NWatch-DShanMCU-F103.uvprojx')
uv_files=[f.findtext('FilePath').replace('\\','/') for f in uv.findall('.//Groups/Group/Files/File')
          if f.findtext('FileOption/CommonProperty/IncludeInBuild')!='0']
uv_files=[p[2:] if p.startswith('./') else p for p in uv_files]
assert len(uv_files)==len(set(uv_files))==48 and set(uv_files)==set(params['sourceList'])
c_includes=uv.findtext('.//Cads/VariousControls/IncludePath').replace('\\','/')
for directory in ['RTOS','ThirdParty/FreeRTOS-Kernel/include','ThirdParty/FreeRTOS-Kernel/portable/RVDS/ARM_CM3']:
    assert directory in c_includes,directory

map_text=(out/'NWatch-DShanMCU-F103.map').read_text()
def symbol(name):
    matches=re.findall(r'^\s+'+re.escape(name)+r'\s+(0x[\da-fA-F]+)\s+(Thumb Code|Data)\s+(\d+)\s+(\S+)',map_text,re.M)
    assert len(matches)==1,(name,matches)
    a,kind,n,obj=matches[0]
    return {'address':int(a,16),'size':int(n),'object':obj,'kind':kind}

handlers={name:symbol(name) for name in ['SVC_Handler','PendSV_Handler','SysTick_Handler',
          'HardFault_Handler','TIM3_IRQHandler','TIM4_IRQHandler','EXTI15_10_IRQHandler']}
for name in ['SVC_Handler','PendSV_Handler','SysTick_Handler']:
    assert handlers[name]['object'].startswith('port.o'),handlers[name]
assert handlers['HardFault_Handler']['object'].startswith('rtos_app.o')
for name in ['TIM3_IRQHandler','TIM4_IRQHandler','EXTI15_10_IRQHandler']:
    assert handlers[name]['object'].startswith('stm32f1xx_it.o')
assert symbol('HAL_InitTick')['object'].startswith('driver_systick.o')
for api in ['uxTaskGetStackHighWaterMark','xPortGetFreeHeapSize',
            'xPortGetMinimumEverFreeHeapSize','vApplicationStackOverflowHook',
            'vApplicationMallocFailedHook','vTaskStartScheduler','vTaskDelay',
            'NWatch_HardFaultCapture','vApplicationTickHook','vApplicationIdleHook']:
    symbol(api)

image={};base=0;records=0
for line in (out/'NWatch-DShanMCU-F103.hex').read_text().splitlines():
    b=bytes.fromhex(line[1:]);assert sum(b)%256==0
    n,offset,kind=b[0],int.from_bytes(b[1:3],'big'),b[3]
    if kind==4:base=int.from_bytes(b[4:4+n],'big')<<16
    elif kind==0:
        for j,value in enumerate(b[4:4+n]):
            addr=base+offset+j
            assert addr not in image
            image[addr]=value
        records+=n
def word(addr):return struct.unpack('<I',bytes(image[addr+j] for j in range(4)))[0]
for name,index in [('SVC_Handler',11),('PendSV_Handler',14),('SysTick_Handler',15),('HardFault_Handler',3),
                   ('TIM3_IRQHandler',16+29),('TIM4_IRQHandler',16+30),('EXTI15_10_IRQHandler',16+40)]:
    assert word(0x08000000+4*index)==handlers[name]['address'],(name,hex(word(0x08000000+4*index)))

def metrics(s):
    return {key:int(re.search(r'Total '+label+r'\s*\([^\n]+?\)\s+(\d+)',s)[1])
            for key,label in [('ro','RO  Size'),('ram','RW  Size'),('flash','ROM Size')]}
old=metrics((stage/'baseline-build.log').read_text())
new=metrics(map_text)
assert records==max(image)-min(image)+1==new['flash']
assert min(image)==0x08000000 and new['flash']<=65536 and new['ram']<=20480
heap=symbol('ucHeap'); assert heap['size']==6144 and heap['object'].startswith('heap_4.o')
c_heap=re.search(r'^\s+(0x[\da-f]+)\s+0x00000200\s+Zero\s+RW\s+\d+\s+HEAP\s+',map_text,re.M)
main_stack=re.search(r'^\s+(0x[\da-f]+)\s+0x00000400\s+Zero\s+RW\s+\d+\s+STACK\s+',map_text,re.M)
assert c_heap and main_stack
ranges=sorted([(heap['address'],heap['address']+6144),
               (int(c_heap[1],16),int(c_heap[1],16)+512),
               (int(main_stack[1],16),int(main_stack[1],16)+1024)])
assert all(a[1]<=b[0] for a,b in zip(ranges,ranges[1:]))
assert word(0x08000000)==int(main_stack[1],16)+1024

asm=(stage/'disassembly.txt').read_text()
create=asm[asm.index('\n    xTaskCreate\n'):]
allocation=re.search(r'MOVS?\s+r0,#0x([\da-f]+)\n[^\n]*BL\s+pvPortMalloc',create)
assert allocation
tcb_size=int(allocation[1],16)
assert tcb_size==64
aligned_start=(heap['address']+7)&~7
end_marker=(heap['address']+6144-8)&~7
alloc=lambda n:(n+8+7)&~7
planned_heap_free=end_marker-aligned_start-alloc(768*4)-alloc(128*4)-2*alloc(tcb_size)
assert planned_heap_free>1024
main=(root/'Core/Src/main.c').read_bytes().decode('latin1')
gui=main[main.index('static void GUI_Task(void *argument)\n{'):]
for call in ['appconfig_init','LCD_Init','draw_init','time_init','buttons_init',
             'pwrmgr_init','display_load','time_update','buttons_update','global_update',
             'mMainUpdate','display_update','pwrmgr_update','vTaskDelay']:
    assert call+'(' in gui,call
app=(mdk/'RTOS/rtos_app.c').read_text()
assert len(re.findall(r'\bxTaskCreate\s*\(',app))==1
assert 'Input_Task' not in app and 'Service_Task' not in app
assert 'No ST-Link detected' in (stage/'stlink-probe.log').read_text()
result={'version':'V11.3.1','port':'portable/RVDS/ARM_CM3','vendor_files_unchanged':vendor_count,
        'baseline':old,'final':new,'free_flash_bytes':65536-new['flash'],'free_ram_bytes':20480-new['ram'],
        'freertos_heap_reserved_bytes':6144,'gui_stack_reserved_words':768,'idle_stack_reserved_words':128,
        'tcb_allocation_bytes_observed_in_ac5_assembly':tcb_size,
        'planned_heap_free_after_gui_and_idle_bytes_NOT_MEASURED':planned_heap_free,
        'runtime_stack_high_water_words':None,'runtime_free_heap_bytes':None,
        'runtime_min_ever_free_heap_bytes':None,'hardware_tested':False,
        'reason':'STM32CubeProgrammer reports no ST-Link detected.',
        'sources':48,'warnings':len(re.findall('Warning:',(out/'compiler.log').read_text())),
        'modified_existing_files':sorted(source_changes),'handlers':handlers,
        'diagnostic_symbols':{name:symbol(name) for name in ['g_nwatchRtos','g_nwatchFault']}}
(stage/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
