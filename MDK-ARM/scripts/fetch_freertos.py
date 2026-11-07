"""Fetch the pinned official release into the current workspace only."""
from pathlib import Path
from urllib.request import Request, build_opener, ProxyHandler
import hashlib
import json
import zipfile
import io

mdk = Path(__file__).resolve().parent.parent
stage = mdk/'phase3_freertos'
tag = 'V11.3.1'
opener = build_opener(ProxyHandler({}))

def get(url):
    req = Request(url, headers={'User-Agent':'NWatch-FreeRTOS-port'})
    with opener.open(req, timeout=40) as response:
        return response.read()

url = 'https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/download/'+tag+'/FreeRTOS-Kernel'+tag+'.zip'
archive = stage/('FreeRTOS-Kernel'+tag+'.zip')
# Pin the verified stable release asset directly; the unauthenticated API may be rate limited.
raw = archive.read_bytes() if archive.exists() else get(url)
sha = hashlib.sha256(raw).hexdigest()
archive.write_bytes(raw)

# Keep the original archive; only the selected port and allocation scheme are built.
dest = mdk/'ThirdParty/FreeRTOS-Kernel'
with zipfile.ZipFile(io.BytesIO(raw)) as z:
    members = [i for i in z.infolist() if not i.is_dir()]
    prefix = members[0].filename.split('/')[0]+'/'
    assert all(i.filename.startswith(prefix) for i in members)
    for info in members:
        rel = Path(info.filename[len(prefix):])
        assert not rel.is_absolute() and '..' not in rel.parts
        p = dest/rel
        data = z.read(info)
        if p.exists():
            assert p.read_bytes()==data, str(rel)+' differs from official archive'
        else:
            p.parent.mkdir(parents=True,exist_ok=True)
            p.write_bytes(data)
assert (dest/'portable/RVDS/ARM_CM3/port.c').is_file()
assert tag in (dest/'include/task.h').read_text()
(stage/'source-origin.json').write_text(json.dumps({
    'version':tag,'official_release':'https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/tag/'+tag,'url':url,
    'archive_sha256':sha,'release_digest':None,
    'metadata_note':'Official stable release asset; SHA256 is locally computed, not a signed upstream digest.',
    'port':'portable/RVDS/ARM_CM3','modified_vendor_files':[]
},indent=2),encoding='utf-8')
print('Official release:',tag)
print('Archive SHA256:',sha)
print('Vendor source:',dest)
