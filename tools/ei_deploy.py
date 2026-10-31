# Build the trained impulse as an Arduino library (int8, EON compiler) and save it to ei_library/
# Usage: python tools/ei_deploy.py
import io
import zipfile

from ei_common import ROOT, call, project_id, wait_job

pid = project_id()
job = call('POST', f'/{pid}/jobs/build-ondevice-model?type=arduino', {'engine': 'tflite-eon', 'modelType': 'int8'})['id']
wait_job(pid, job, 'build')
data = call('GET', f'/{pid}/deployment/download?type=arduino&modelType=int8&engine=tflite-eon', raw=True)
out = ROOT / 'ei_library'
out.mkdir(exist_ok=True)
(out / 'ei-arduino-library.zip').write_bytes(data)
names = zipfile.ZipFile(io.BytesIO(data)).namelist()
print(f'saved {out / "ei-arduino-library.zip"} ({len(data) // 1024} KB), library folder: {names[0].split("/")[0]}')
