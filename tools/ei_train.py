# Generate features and train the image classifier in the Edge Impulse project from .env
# Impulse: 96x96 RGB image -> Image block (id 2) -> MobileNetV1 0.25 transfer learning (id 3), int8 for ESP32-S3
# (MobileNetV2 0.35 scored 100% float32 but only 80% int8 on the test set; V1 0.25 kept 100% after quantization)
# Usage: python tools/ei_train.py [model] [training cycles]
#   e.g. python tools/ei_train.py transfer_mobilenetv2_a35 40
import json
import sys

from ei_common import call, project_id, wait_job

DSP_ID, LEARN_ID = 2, 3
MODEL = sys.argv[1] if len(sys.argv) > 1 else 'transfer_mobilenetv1_a25_d100'
CYCLES = int(sys.argv[2]) if len(sys.argv) > 2 else 60

pid = project_id()
call('POST', f'/{pid}/dsp/{DSP_ID}', {'config': {'channels': 'RGB'}})
job = call('POST', f'/{pid}/jobs/generate-features', {'dspId': DSP_ID})['id']
wait_job(pid, job, 'features')

job = call('POST', f'/{pid}/jobs/train/keras/{LEARN_ID}', {
    'mode': 'visual',
    'trainingCycles': CYCLES,
    'learningRate': 0.0005,
    'augmentationPolicyImage': 'all',  # random flips / crops / brightness, helps with 80 images per class
    'visualLayers': [{'type': MODEL, 'neurons': 16, 'dropoutRate': 0.1}],
    'profileInt8': True,
})['id']
wait_job(pid, job, 'training')

meta = call('GET', f'/{pid}/training/keras/{LEARN_ID}/metadata')
for m in meta.get('modelValidationMetrics', []):
    print(json.dumps({k: m.get(k) for k in ('type', 'accuracy', 'loss', 'confusionMatrix')}))

# Accuracy on the held-out testing set (the 20% the model never saw)
job = call('POST', f'/{pid}/jobs/classify', {'modelVariants': ['int8', 'float32']})['id']
wait_job(pid, job, 'testing')
for m in call('GET', f'/{pid}/classify/all/metrics')['metrics']:
    print(f"test accuracy {m['variant']}: {m['accuracy']:.1f}%")
