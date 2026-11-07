"""只验证本次构建产物；所有未读取的硬件运行值保持为空。"""
from pathlib import Path
import hashlib
import json
import re
import struct
import zipfile

stage = Path(__file__).resolve().parent
mdk = stage.parent
out = mdk / 'build/NWatch-DShanMCU-F103'
name = 'NWatch-DShanMCU-F103'
sha = lambda data: hashlib.sha256(data).hexdigest()
params = json.loads((out / 'builder.params').read_text())
assert params['toolchain'] == 'AC5'
assert (params['rom'], params['ram']) == (65536, 20480)
assert len(params['sourceList']) == len(set(params['sourceList'])) == 48
expected_kernel = {'ThirdParty/FreeRTOS-Kernel/' + p for p in (
    'tasks.c', 'list.c', 'queue.c', 'portable/RVDS/ARM_CM3/port.c', 'portable/MemMang/heap_4.c')}
assert {p for p in params['sourceList'] if p.startswith('ThirdParty/')} == expected_kernel

# 按本轮开始时的哈希检查 UI、驱动和工程配置未被修改。
before = json.loads((stage / 'source-hashes-before.json').read_text())
changes = [p for p, digest in before.items() if sha((mdk / p).read_bytes()) != digest]
assert set(changes) == {'RTOS/rtos_app.c', 'RTOS/rtos_app.h'}, changes
origin = json.loads((mdk / 'phase3_freertos/source-origin.json').read_text())
archive = mdk / 'phase3_freertos/FreeRTOS-KernelV11.3.1.zip'
assert sha(archive.read_bytes()) == origin['archive_sha256']
vendor_count = 0
with zipfile.ZipFile(archive) as z:
    for item in z.infolist():
        if not item.is_dir():
            relative = item.filename.split('/', 1)[1]
            assert (mdk / 'ThirdParty/FreeRTOS-Kernel' / relative).read_bytes() == z.read(item), relative
            vendor_count += 1

map_text = (out / (name + '.map')).read_text()
def symbol(name):
    found = re.findall(r'^\s+' + re.escape(name) + r'\s+(0x[\da-fA-F]+)\s+(Thumb Code|Data)\s+(\d+)\s+(\S+)', map_text, re.M)
    assert len(found) == 1, (name, found)
    address, kind, size, obj = found[0]
    return dict(address=int(address, 16), size=int(size), object=obj, kind=kind)

vector_indices = {'SVC_Handler': 11, 'PendSV_Handler': 14, 'SysTick_Handler': 15,
                  'HardFault_Handler': 3, 'TIM3_IRQHandler': 45,
                  'TIM4_IRQHandler': 46, 'EXTI15_10_IRQHandler': 56}
handlers = {n: symbol(n) for n in vector_indices}
for n in ('SVC_Handler', 'PendSV_Handler', 'SysTick_Handler'):
    assert handlers[n]['object'].startswith('port.o')
assert handlers['HardFault_Handler']['object'].startswith('rtos_app.o')
for n in ('TIM3_IRQHandler', 'TIM4_IRQHandler', 'EXTI15_10_IRQHandler'):
    assert handlers[n]['object'].startswith('stm32f1xx_it.o')
assert symbol('HAL_InitTick')['object'].startswith('driver_systick.o')
apis = ('GUI_Task', 'prvIdleTask', 'xTaskCreate', 'vTaskStartScheduler', 'xTaskGetSchedulerState',
        'xTaskGetTickCount', 'vTaskDelay', 'vTaskSwitchContext', 'uxTaskGetStackHighWaterMark',
        'xPortGetFreeHeapSize', 'xPortGetMinimumEverFreeHeapSize', 'vApplicationTickHook',
        'vApplicationIdleHook', 'vApplicationMallocFailedHook', 'vApplicationStackOverflowHook')
for n in apis:
    symbol(n)
assert not re.search(r'^\s+HAL_Delay\s+0x', map_text, re.M)

# 独立核对 HEX 校验和、地址范围与 AXF 的实际装载字节。
image = {}
base = 0
eof = False
for line in (out / (name + '.hex')).read_text().splitlines():
    assert line.startswith(':')
    b = bytes.fromhex(line[1:])
    assert len(b) == b[0] + 5 and sum(b) % 256 == 0
    count, offset, kind = b[0], int.from_bytes(b[1:3], 'big'), b[3]
    if kind == 4:
        base = int.from_bytes(b[4:4 + count], 'big') << 16
    elif kind == 0:
        for i, value in enumerate(b[4:4 + count]):
            address = base + offset + i
            assert address not in image
            image[address] = value
    elif kind == 1:
        eof = True
    else:
        assert kind == 5
assert eof and min(image) == 0x08000000
assert len(image) == max(image) - min(image) + 1 <= 65536
word = lambda address: struct.unpack('<I', bytes(image[address + j] for j in range(4)))[0]
for n, index in vector_indices.items():
    assert word(0x08000000 + 4 * index) == handlers[n]['address']
elf = (out / (name + '.axf')).read_bytes()
assert elf[:6] == b'\x7fELF\x01\x01'
phoff, = struct.unpack_from('<I', elf, 28)
phsize, phnum = struct.unpack_from('<HH', elf, 42)
loads = [struct.unpack_from('<8I', elf, phoff + i * phsize) for i in range(phnum)]
loads = [p for p in loads if p[0] == 1]
assert len(loads) == 1
_, offset, virtual, physical, size, _, _, _ = loads[0]
assert physical == virtual == min(image) and size == len(image)
assert elf[offset:offset + size] == bytes(image[physical + i] for i in range(size))

flash = int(re.search(r'Total ROM Size[^\n]+?\)\s+(\d+)', map_text)[1])
ram = int(re.search(r'Total RW  Size[^\n]+?\)\s+(\d+)', map_text)[1])
assert flash == len(image) and ram <= 20480
heap = symbol('ucHeap')
assert heap['size'] == 6144 and heap['object'].startswith('heap_4.o')
asm = (stage / 'disassembly.txt').read_text()
create = asm[asm.index('\n    xTaskCreate\n'):]
tcb = int(re.search(r'MOVS?\s+r0,#0x([\da-f]+)\n[^\n]*BL\s+pvPortMalloc', create)[1], 16)
assert tcb == 64
aligned_start = (heap['address'] + 7) & ~7
end_marker = (heap['address'] + 6144 - 8) & ~7
block_size = lambda n: (n + 8 + 7) & ~7
planned_free = end_marker - aligned_start - block_size(768 * 4) - block_size(128 * 4) - 2 * block_size(tcb)
main = (mdk.parent / 'Core/Src/main.c').read_bytes().decode('latin1').replace('\r\n', '\n')
assert 'vTaskDelay(pdMS_TO_TICKS(1U))' in main
app = (mdk / 'RTOS/rtos_app.c').read_text(encoding='utf-8')
assert len(re.findall(r'\bxTaskCreate\s*\(', app)) == 1
assert 'No ST-Link detected' in (stage / 'stlink-probe.log').read_text(encoding='utf-8-sig')
result = dict(static_checks_passed=True, hardware_tested=False,
    hardware_reason='本次 CubeProgrammer 枚举未发现 ST-Link，未烧录。',
    toolchain='AC5 5.06u5 build 528', version='V11.3.1', sources=48,
    warnings=(stage / 'rebuild.log').read_text(encoding='utf-8-sig').count('Warning:'),
    unchanged_vendor_files=vendor_count, tracked_files=len(before), modified_files=changes,
    flash_load_bytes=flash, flash_free_bytes=65536-flash, ram_rw_zi_bytes=ram, ram_free_bytes=20480-ram,
    freertos_heap_bytes=6144, gui_stack_words=768, gui_stack_bytes=3072, idle_stack_words=128,
    stack_word_bytes=4, tcb_bytes=tcb, heap_free_estimate_NOT_MEASURED=planned_free,
    runtime=dict(taskCreateResult=None, schedulerState=None, freeHeapBytes=None,
        minEverFreeHeapBytes=None, guiStackHighWaterWords=None, guiIterationsIncreasing=None,
        tickIncreasing=None, fault=None), handlers=handlers,
    diagnostic_symbols={n: symbol(n) for n in ('g_nwatchRtos','g_nwatchFault')},
    artifact_sha256={suffix: sha((out / (name+suffix)).read_bytes()) for suffix in ('.axf','.hex','.map')})
(stage / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False, indent=2))
