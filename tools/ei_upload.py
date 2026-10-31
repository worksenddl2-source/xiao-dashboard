# Upload dataset/<class>/*.jpg to the Edge Impulse project in .env (80% training / 20% testing)
# Usage: python tools/ei_upload.py
import random
import urllib.request
import uuid

from ei_common import KEY, ROOT

INGEST = 'https://ingestion.edgeimpulse.com/api/{}/files'


def upload(category, label, files):
    boundary = uuid.uuid4().hex
    body = b''
    for f in files:
        body += (f'--{boundary}\r\nContent-Disposition: form-data; name="data"; filename="{f.name}"\r\n'
                 'Content-Type: image/jpeg\r\n\r\n').encode() + f.read_bytes() + b'\r\n'
    body += f'--{boundary}--\r\n'.encode()
    req = urllib.request.Request(INGEST.format(category), data=body, method='POST', headers={
        'x-api-key': KEY, 'x-label': label, 'x-disallow-duplicates': '1',
        'Content-Type': f'multipart/form-data; boundary={boundary}'})
    urllib.request.urlopen(req).read()


random.seed(42)
for folder in sorted(p for p in (ROOT / 'dataset').iterdir() if p.is_dir() and p.name != 'zips'):
    files = sorted(folder.glob('*.jpg'))
    random.shuffle(files)
    cut = round(len(files) * 0.8)
    for category, part in (('training', files[:cut]), ('testing', files[cut:])):
        for i in range(0, len(part), 20):
            upload(category, folder.name, part[i:i + 20])
        print(f'{folder.name}: {len(part)} -> {category}')
