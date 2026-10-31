# HttpDashboard: FastAPI + Supabase + HTTP (어디서나 제어)

```
[XIAO] --HTTPS POST /device/report (1초마다)--> [FastAPI on Render] <--HTTPS-- [웹 on Vercel]
                 <-- 응답: {"led": true} --              |
                                                    [Supabase: Auth + api_device 테이블]
```

보드와 브라우저가 모두 인터넷의 서버로 HTTP 요청을 보내기 때문에 **같은 와이파이가 아니어도** 됩니다.
보드는 서버 응답으로 LED 상태를 받아서 적용합니다.

| 구성 | 주소 |
|---|---|
| 웹 대시보드 (Vercel) | https://xiao-http-dashboard.vercel.app |
| 백엔드 API (Render) | https://xiao-http-api.onrender.com |
| API 문서 / 테스트 (Swagger) | https://xiao-http-api.onrender.com/docs |

## API

| 메서드 | 경로 | 인증 | 설명 |
|---|---|---|---|
| GET | `/` | - | 서버 상태 |
| POST | `/auth/signup` | - | 회원가입 `{"email","password"}` (이메일 인증 필요) |
| POST | `/auth/login` | - | 로그인 → `access_token`, `refresh_token` |
| POST | `/auth/refresh` | - | 토큰 갱신 `{"refresh_token"}` |
| GET | `/auth/me` | Bearer | 내 정보 |
| GET | `/api/device` | Bearer | 센서값·LED 상태·마지막 보고 시각 |
| POST | `/api/led` | Bearer | LED 제어 `{"on": true}` |
| POST | `/device/report` | `X-Device-Key` | 보드 전용: 센서값 보고, 응답으로 LED 상태 |

로그인 안 한 사용자는 센서값을 볼 수도, LED를 바꿀 수도 없습니다(Supabase RLS). 보드는 장치 키로만 보고할 수 있고, DB에는 키의 SHA-256 해시만 저장됩니다.

## 폴더

```
HttpDashboard/
├── HttpDashboard.ino   ← 보드 펌웨어 (HTTPS로 보고 + LED 적용)
├── secrets.example.h   ← 와이파이 / API 주소 / 장치 키 예시 → secrets.h로 복사
├── schema.sql          ← Supabase 테이블, 권한, 보고 함수
├── backend/            ← FastAPI (Render에 배포)
└── frontend/           ← 정적 웹 (Vercel에 배포)
```

## 로컬 개발

```bash
cd HttpDashboard/backend
pip install -r requirements.txt
python -m uvicorn main:app --port 8100      # http://127.0.0.1:8100/docs

cd HttpDashboard/frontend
python -m http.server 5500                  # http://localhost:5500 → 자동으로 로컬 백엔드 사용
```

## 배포

**백엔드 (Render CLI)**: GitHub 저장소의 코드를 Render가 받아서 실행합니다. 처음 한 번만 서비스를 만듭니다.
```bash
render login
render services create --name xiao-http-api --type web_service \
  --repo https://github.com/worksenddl2-source/xiao-dashboard --branch http-dashboard \
  --root-directory HttpDashboard/backend --runtime python --plan free --region singapore \
  --build-command "pip install -r requirements.txt" \
  --start-command "uvicorn main:app --host 0.0.0.0 --port \$PORT" \
  --health-check-path / --env-var PYTHON_VERSION=3.12.8
```
- 이후에는 브랜치에 push하면 자동으로 다시 배포됩니다.
- ⚠️ Git Bash에서는 `/`가 `C:/Program Files/Git/`으로 바뀌어 버립니다. 앞에 `MSYS_NO_PATHCONV=1`을 붙이세요.
- 무료 플랜은 15분 동안 요청이 없으면 잠들고, 깨어나는 데 1분 정도 걸립니다. 보드가 1초마다 보고하는 동안은 잠들지 않습니다.

**프론트엔드 (Vercel CLI)**
```bash
cd HttpDashboard/frontend
vercel deploy --prod
```

## 보드

1. `secrets.example.h` → `secrets.h` 복사 후 와이파이, `API_URL`, `DEVICE_KEY`(아무 긴 랜덤 문자열) 입력
2. Supabase SQL Editor에서 키 등록:
   ```sql
   insert into public.api_device_key values
     ('xiao', encode(extensions.digest('<DEVICE_KEY>', 'sha256'), 'hex'));
   ```
3. 업로드: `arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3 HttpDashboard`

시리얼 모니터에 `report failed: 403`이 나오면 장치 키가 틀린 것이고, `502`/`503`은 서버가 깨어나는 중입니다.
