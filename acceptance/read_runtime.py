"""通过已有 OpenOCD 短暂停机取快照，间隔连续运行；不写内存、不复位。"""
from pathlib import Path
import hashlib
import json
import socket
import struct
import time

stage = Path(__file__).resolve().parent
mdk = stage.parent
fields = ('magic phase guiIterations kernelTickCount tickHookCount idleHookCount taskCount '
          'guiStackWords idleStackWords guiStackHighWaterWords idleStackHighWaterWords '
          'heapTotalBytes freeHeapBytes minEverFreeHeapBytes halMilliseconds microsecondsLow '
          'controlRegister processStackPointer mainStackPointer basepri systickReload '
          'systickControl schedulerState taskCreateResult').split()
with socket.create_connection(('127.0.0.1', 6666), timeout=8) as connection:
    connection.settimeout(20)
    def command(text):
        connection.sendall(text.encode('ascii') + b'\x1a')
        response = bytearray()
        while not response.endswith(b'\x1a'):
            part = connection.recv(262144)
            if not part:
                raise RuntimeError('OpenOCD connection closed')
            response.extend(part)
        return response[:-1].decode('utf-8').strip()

    def read32(address, count):
        values = [int(n, 0) for n in command(f'read_memory {address:#x} 32 {count}').split()]
        assert len(values) == count
        return values

    state = command('stm32f1x.cpu curstate')
    assert state == 'running', state
    # 使用已在暂停状态读回的完整 Flash；本机运行中大块读回不可靠。
    verified = json.loads((stage / 'verification.json').read_text(encoding='utf-8'))
    size = verified['flash_load_bytes']
    assert size % 4 == 0
    flash = (stage / 'board-flash-readback.bin').read_bytes()
    elf = (mdk / 'build/NWatch-DShanMCU-F103/NWatch-DShanMCU-F103.axf').read_bytes()
    header = struct.unpack_from('<8I', elf, struct.unpack_from('<I', elf, 28)[0])
    expected = elf[header[1]:header[1] + header[4]]
    assert flash == expected, 'Board Flash does not match current AXF'
    samples = []
    for index in range(3):
        if index:
            time.sleep(3)
        start = time.monotonic()
        command('halt')
        try:
            assert command('stm32f1x.cpu curstate') == 'halted'
            diagnostic = dict(zip(fields, read32(0x20000C68, len(fields))))
            overflow = read32(0x20000028, 2)
            sample = dict(monotonic_seconds=start,
                      diagnostic=diagnostic, fault_words=read32(0x20000CC8, 17),
                      kernel_tick=read32(0x20000130, 1)[0],
                      hal_tick=read32(0x20000038, 1)[0],
                      micro_overflow_us=overflow[0] + (overflow[1] << 32),
                      tim3_cnt=read32(0x40000424, 1)[0],
                      tim4_cnt=read32(0x40000824, 1)[0],
                      cfsr=read32(0xE000ED28, 1)[0], hfsr=read32(0xE000ED2C, 1)[0],
                      core_state=command('stm32f1x.cpu curstate'))
        finally:
            command('resume')
        sample['state_after_resume'] = command('stm32f1x.cpu curstate')
        assert diagnostic['magic'] == 0x4E575254 and diagnostic['schedulerState'] == 2
        assert diagnostic['taskCreateResult'] == 1 and diagnostic['phase'] == 3
        assert diagnostic['taskCount'] == 2 and diagnostic['guiStackHighWaterWords'] > 0
        assert sample['fault_words'][0] == sample['cfsr'] == sample['hfsr'] == 0
        assert sample['core_state'] == 'halted' and sample['state_after_resume'] == 'running'
        samples.append(sample)
    deltas = []
    for first, last in zip(samples, samples[1:]):
        d = {n: (last['diagnostic'][n] - first['diagnostic'][n]) & 0xffffffff
             for n in ('guiIterations', 'kernelTickCount', 'tickHookCount', 'idleHookCount',
                       'halMilliseconds', 'microsecondsLow')}
        d['kernel_tick'] = (last['kernel_tick'] - first['kernel_tick']) & 0xffffffff
        d['hal_tick'] = (last['hal_tick'] - first['hal_tick']) & 0xffffffff
        d['micro_timer_us'] = (last['micro_overflow_us'] + last['tim3_cnt']
                              - first['micro_overflow_us'] - first['tim3_cnt'])
        d['wall_seconds'] = last['monotonic_seconds'] - first['monotonic_seconds']
        assert d['guiIterations'] > 0 and d['idleHookCount'] > 0 and d['kernel_tick'] > 0
        deltas.append(d)
    result = dict(hardware_tested=True, reset_performed_by_sampler=False, firmware_written=False,
                  snapshot_method='halt-read-resume', halted_snapshot_count=3,
                  flash_matches_axf=True, flash_load_bytes=size,
                  flash_sha256=hashlib.sha256(flash).hexdigest(), samples=samples, deltas=deltas,
                  final_core_state=command('stm32f1x.cpu curstate'))
    (stage / 'runtime-observations.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False, indent=2))
