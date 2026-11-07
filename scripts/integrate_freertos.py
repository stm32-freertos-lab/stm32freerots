"""Move the existing NWatch loop into one task; preserve original source bytes."""
from pathlib import Path
import re
import hashlib
import json

mdk = Path(__file__).resolve().parent.parent
root = mdk.parent
assert root.name == 'NWatch-DShanMCU-F103-RTOS'
manifest=json.loads((mdk/'phase3_freertos/baseline/manifest.json').read_text())

def read(rel):
    b=(root/rel).read_bytes()
    assert hashlib.sha256(b).hexdigest()==manifest[rel], rel+' changed since snapshot'
    return b.decode('latin1').replace('\r\n','\n')

def cn(s):
    return s.encode('utf-8').decode('latin1')

def put(rel,s):
    (root/rel).write_bytes(s.replace('\n','\r\n').encode('latin1'))

s=read('Core/Src/main.c')
s=s.replace('#include "driver_timer.h"', '#include "driver_timer.h"\n#include "FreeRTOS.h"\n#include "task.h"\n#include "rtos_app.h"',1)
s=s.replace('/* USER CODE BEGIN PFP */','/* USER CODE BEGIN PFP */\nstatic void GUI_Task(void *argument);',1)
init_start=s.index('  /* USER CODE BEGIN 2 */')
init_end=s.index('  /* USER CODE END 2 */',init_start)
init=s[init_start+len('  /* USER CODE BEGIN 2 */'):init_end]
loop_start=s.index('    /* USER CODE BEGIN 3 */',init_end)
loop_end=s.index('\n  }\n  /* USER CODE END 3 */',loop_start)
loop=s[loop_start+len('    /* USER CODE BEGIN 3 */'):loop_end]
main_end=s.index('\n}',loop_end)+2
replacement=cn('''  /* USER CODE BEGIN 2 */
  /* 所有 NWatch 初始化/绘图归 GUI 任务；调度失败时不回退到裸机循环。 */
  NWatch_RtosStart(GUI_Task);
  /* USER CODE END 2 */
}

static void GUI_Task(void *argument)
{
  (void)argument;
  NWatch_RtosGuiEntered();
#if !NWATCH_RTOS_BRINGUP_ONLY
''')+init+'''  NWatch_RtosGuiReady();
#endif
  for(;;)
  {
#if !NWATCH_RTOS_BRINGUP_ONLY
'''+loop+cn('''
#endif
    NWatch_RtosPoll();
    /* 帧率仍由原 display_update 和 TIM4 控制；每轮至少让出一个内核 Tick。 */
    vTaskDelay(pdMS_TO_TICKS(1U));
  }
}''')
s=s[:init_start]+replacement+s[main_end:]
pat=r'void Error_Handler\(void\)\n\{.*?\n\}'
s,n=re.subn(pat,'''void Error_Handler(void)
{
  NWatch_RtosFatal(NWATCH_FAULT_HAL, __FILE__, __LINE__);
}''',s,count=1,flags=re.S)
assert n==1
put('Core/Src/main.c',s)

s=read('Core/Src/stm32f1xx_it.c')
s=s.replace('#include "driver_timer.h"','#include "driver_timer.h"\n#include "rtos_app.h"',1)
for name in ['SVC_Handler','PendSV_Handler','SysTick_Handler','HardFault_Handler']:
    pattern=r'void '+name+r'\(void\)\n\{.*?\n\}'
    note='/* '+name+cn(' 由 RTOS 移植层或故障捕获入口直接提供，禁止重复定义。')+' */'
    s,n=re.subn(pattern,lambda _:note,s,count=1,flags=re.S)
    assert n==1,name
for name,reason in [('NMI_Handler','NMI'),('MemManage_Handler','MEMMANAGE'),
                    ('BusFault_Handler','BUSFAULT'),('UsageFault_Handler','USAGEFAULT')]:
    pattern=r'void '+name+r'\(void\)\n\{.*?\n\}'
    body='void '+name+'(void)\n{\n  NWatch_RtosFatal(NWATCH_FAULT_'+reason+', __FILE__, __LINE__);\n}'
    s,n=re.subn(pattern,lambda _:body,s,count=1,flags=re.S)
    assert n==1,name
put('Core/Src/stm32f1xx_it.c',s)
print('Integrated one GUI task; removed duplicate SVC/PendSV/SysTick/HardFault definitions.')
