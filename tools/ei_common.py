# Shared helpers for the Edge Impulse scripts: reads EI_API_KEY from ../.env
import json
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
API = 'https://studio.edgeimpulse.com/v1/api'


def api_key():
    for line in (ROOT / '.env').read_text(encoding='utf-8').splitlines():
        if line.startswith('EI_API_KEY='):
            return line.split('=', 1)[1].strip()
    raise SystemExit('EI_API_KEY not found in .env')


KEY = api_key()


def call(method, path, body=None, base=API, raw=False):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(base + path, data=data, method=method,
                                 headers={'x-api-key': KEY, 'Content-Type': 'application/json'})
    with urllib.request.urlopen(req) as r:
        out = r.read()
    if raw:
        return out
    res = json.loads(out)
    if res.get('success') is False:
        raise RuntimeError(f'{method} {path}: {res.get("error")}')
    return res


def project_id():
    return call('GET', '/projects')['projects'][0]['id']


def wait_job(pid, job_id, label):
    print(f'{label}: job {job_id}', flush=True)
    while True:
        job = call('GET', f'/{pid}/jobs/{job_id}/status')['job']
        if job.get('finished'):
            if not job.get('finishedSuccessful'):
                stdout = call('GET', f'/{pid}/jobs/{job_id}/stdout?limit=40').get('stdout', [])
                print('\n'.join(s.get('data', '') for s in stdout[-40:]))
                raise SystemExit(f'{label} failed')
            print(f'{label}: done', flush=True)
            return
        time.sleep(5)
