"""Check linked capacity, configuration consistency and unchanged UI assets.

This is a static/build-artifact check, not a board or runtime simulation test.
"""
from pathlib import Path
import hashlib
import json
import re
import xml.etree.ElementTree as ET

mdk = Path(__file__).resolve().parent.parent
root = mdk.parent
before = mdk/'phase1/before'
out = mdk/'build/NWatch-DShanMCU-F103'
params = json.loads((out/'builder.params').read_text())
disabled = json.loads((mdk/'phase1/excluded-files.json').read_text())
sources = set(params['sourceList'])
assert params['toolchain'] == 'AC5'
assert params['rom'] == 65536 and params['ram'] == 20480
assert params['options']['c/cpp-compiler']['optimization'] == 'level-3'
assert not any(Path(s).name in disabled for s in sources)
assert len(sources) == 40

uv = ET.parse(mdk/'NWatch-DShanMCU-F103.uvprojx')
uv_excluded = {f.findtext('FileName') for f in uv.findall('.//Groups/Group/Files/File')
               if f.findtext('FileOption/CommonProperty/IncludeInBuild') == '0'}
assert set(disabled) == uv_excluded
active_uv = {f.findtext('FilePath').replace('\\','/')
             for f in uv.findall('.//Groups/Group/Files/File')
             if f.findtext('FileName') not in uv_excluded}
active_uv = {p[2:] if p.startswith('./') else p for p in active_uv}
assert sources == active_uv, (sources-active_uv, active_uv-sources)

def metrics(p):
    s = p.read_text()
    return {key: int(re.search(r'Total '+label+r'\s*\([^\n]+?\)\s+(\d+)', s)[1])
            for key,label in [('ro','RO  Size'),('ram','RW  Size'),('flash','ROM Size')]}

old = metrics(mdk/'phase1/baseline.map')
new = metrics(out/'NWatch-DShanMCU-F103.map')
assert new['flash'] < old['flash'] and new['ram'] < old['ram']
assert new['flash'] <= 65536 and new['ram'] <= 20480
map_text = (out/'NWatch-DShanMCU-F103.map').read_text()
for name in disabled:
    assert Path(name).stem+'.o' not in map_text, name
sym = re.search(r'cubeVertices\s+(0x[0-9a-fA-F]+)\s+Data\s+4320\s+watchface.o',map_text)
assert sym and 0x08000000 <= int(sym[1],16) < 0x08010000

def flash_span(p):
    base = 0
    addresses = []
    total = 0
    for line in p.read_text().splitlines():
        b = bytes.fromhex(line[1:])
        assert sum(b)%256 == 0
        size, offset, kind = b[0], int.from_bytes(b[1:3],'big'), b[3]
        data = b[4:4+size]
        if kind == 4:
            base = int.from_bytes(data,'big')<<16
        elif kind == 0:
            total += size
            addresses.extend([base+offset, base+offset+size-1])
    assert min(addresses) == 0x08000000
    return total, max(addresses)-min(addresses)+1

assert flash_span(mdk/'phase1/baseline.hex') == (old['flash'],old['flash'])
assert flash_span(out/'NWatch-DShanMCU-F103.hex') == (new['flash'],new['flash'])

def table(p, name):
    s = p.read_bytes().decode('latin1')
    value = re.search(r'uint8_t '+name+r'\[1440\]\[3\]\s*=\s*\{(.*?)\};',s,re.S)[1]
    return bytes(map(int, re.findall(r'\d+',value)))

cube = table(root/'NWatch/watchface.c','cubeVertices')
assert cube == table(before/'NWatch/watchface.c','vertices')
assert cube == table(before/'NWatch/tunemaker.c','vertices')
assert len(cube) == 4320

def resources(p):
    s = p.read_bytes().decode('latin1')
    return {m[1]: re.sub(r'\s+','',m[0]) for m in re.finditer(
        r'const (?:byte|char) (\w+)\[.*?\};',s,re.S)}

old_resources = resources(before/'NWatch/resources.c')
new_resources = resources(root/'NWatch/resources.c')
assert old_resources == new_resources
for name in ['menu_tunemaker','menu_display','menu_settings','menu_exit',
             'menu_anim','selectbar_top','selectbar_bottom','smallFont',
             'midFont','small2Font','usbIcon','battIconEmpty']:
    assert name in new_resources

for name in ['animation.c','display.c','draw.c']:
    assert '../NWatch/'+name in sources

changed = sorted(str(p.relative_to(before)).replace('\\','/')
                 for p in before.rglob('*') if p.is_file() and
                 p.read_bytes() != (root/p.relative_to(before)).read_bytes())
warnings = len(re.findall(r'Warning:', (out/'compiler.log').read_text()))
result = {
    'compiler':'ARM Compiler 5.06 update 5 (build 528)',
    'baseline':old, 'phase1':new,
    'saved_flash':old['flash']-new['flash'], 'saved_ram':old['ram']-new['ram'],
    'free_flash':65536-new['flash'], 'free_ram':20480-new['ram'],
    'warnings':warnings, 'compiled_sources':len(sources),
    'excluded_sources':len(disabled),
    'cube_table_address':sym[1], 'cube_sha256':hashlib.sha256(cube).hexdigest(),
    'changed_files':changed,
    'checks':['AC5/O3 and 64KB/20KB limits', 'EIDE/uVision source parity',
              'excluded modules absent from link map', 'HEX checksums/address span',
              'one Flash cube table with unchanged 180 frames',
              'original resource array definitions unchanged'],
    'hardware_tested':False
}
(mdk/'phase1/verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
