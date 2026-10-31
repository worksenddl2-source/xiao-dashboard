# XIAO ESP32-S3 실습 모음

Seeed Studio **XIAO ESP32-S3 (Sense)** 보드 하나로 LED 깜빡이기부터 웹 대시보드, 클라우드(Supabase), 카메라 스트리밍, Tailscale 원격 접속, AP 모드, 블루투스(BLE) 제어, Edge Impulse로 카메라 사진 분류 모델 학습까지 단계별로 만들어 본 기록입니다.

모든 업로드는 **Arduino IDE 없이 `arduino-cli`(명령어)** 로 했습니다. 처음 해 보는 분도 따라 할 수 있도록 각 단계마다 "무엇을 하는지 → 어떻게 올리는지 → 어떻게 쓰는지 → 안 될 때"를 적었습니다.

---

## 목차

1. [한눈에 보기](#1-한눈에-보기)
2. [준비물](#2-준비물)
3. [폴더 구조](#3-폴더-구조)
4. [개발 환경 만들기 (arduino-cli)](#4-개발-환경-만들기-arduino-cli)
5. [먼저 알아두면 좋은 개념](#5-먼저-알아두면-좋은-개념)
6. [단계별 실습](#6-단계별-실습)
   - [6-1. Blink: LED 깜빡이기](#6-1-blink-led-깜빡이기)
   - [6-2. TouchLED: 터치하면 LED 켜기](#6-2-touchled-터치하면-led-켜기)
   - [6-3. WebDashboard: 같은 와이파이에서 웹으로 제어](#6-3-webdashboard-같은-와이파이에서-웹으로-제어)
   - [6-4. SupabaseDashboard: 어디서나 제어 + 내부망 대시보드 + 카메라](#6-4-supabasedashboard-어디서나-제어--내부망-대시보드--카메라)
   - [6-5. Tailscale: 밖에서 내부망 대시보드·카메라 보기](#6-5-tailscale-밖에서-내부망-대시보드카메라-보기)
   - [6-6. APDashboard: 보드가 직접 와이파이를 뿌리기](#6-6-apdashboard-보드가-직접-와이파이를-뿌리기)
   - [6-7. BLEControl: 스마트폰과 블루투스로 직접 연결](#6-7-blecontrol-스마트폰과-블루투스로-직접-연결)
   - [6-8. EICollector + Edge Impulse: 카메라 사진으로 물체 분류 모델 만들기](#6-8-eicollector--edge-impulse-카메라-사진으로-물체-분류-모델-만들기)
7. [문제 해결 (FAQ)](#7-문제-해결-faq)
8. [보안 주의사항](#8-보안-주의사항)
9. [참고 자료](#9-참고-자료)

---

## 1. 한눈에 보기

| 단계 | 폴더 | 하는 일 | 연결 방식 | 인터넷 필요 |
|---|---|---|---|---|
| 1 | `Blink` | LED 0.1초 간격 깜빡이기 | USB | ✗ |
| 2 | `TouchLED` | D0 핀을 만지면 LED 켜짐 | USB | ✗ |
| 3 | `WebDashboard` | 같은 와이파이에서 웹으로 LED·터치 | 와이파이 (공유기) | ✗ |
| 4 | `SupabaseDashboard` + `web/` | **외부망 대시보드** + **내부망 대시보드** + 카메라 | 와이파이 + Supabase | ✔ (외부망) |
| 5 | (설정만) | Tailscale로 밖에서 내부망 대시보드·카메라 보기 | Tailscale | ✔ |
| 6 | `APDashboard` | 보드가 와이파이를 직접 만들고 웹 서버 실행 | 보드 AP | ✗ |
| 7 | `BLEControl` + `web/ble.html` | 스마트폰과 블루투스로 LED·터치 | BLE | 페이지 열 때만 |
| 8 | `EICollector` + `tools/ei_*.py` | 카메라로 사진 수집 → Edge Impulse로 분류 모델 학습 | 와이파이 (공유기) | ✔ (학습할 때) |

**바로 쓸 수 있는 주소**

| 대시보드 | 주소 | 필요한 펌웨어 |
|---|---|---|
| 외부망 대시보드 (어디서나) | https://worksenddl2-source.github.io/xiao-dashboard/ | `SupabaseDashboard` |
| 내부망 대시보드 (같은 와이파이 / Tailscale) | `http://<보드 IP>/` (예: http://10.22.149.227/) | `SupabaseDashboard` |
| AP 대시보드 | http://192.168.4.1/ (보드 와이파이에 접속 후) | `APDashboard` |
| BLE 대시보드 | https://worksenddl2-source.github.io/xiao-dashboard/web/ble.html | `BLEControl` |
| 사진 수집 대시보드 | `http://<보드 IP>/` 또는 http://xiao.local/ (같은 와이파이) | `EICollector` |

> ⚠️ **보드에는 펌웨어가 한 번에 하나만 들어갑니다.** 예를 들어 `BLEControl`을 올리면 `SupabaseDashboard` 기능(외부망·내부망 대시보드, 카메라)은 멈춥니다. 다시 쓰려면 해당 폴더를 다시 업로드하면 됩니다.

---

## 2. 준비물

### 하드웨어
- **Seeed Studio XIAO ESP32-S3** (카메라를 쓰려면 **Sense** 확장보드 필요)
- **USB-C 데이터 케이블**
  - ⚠️ "충전 전용" 케이블은 전원만 들어오고 데이터가 안 통해서 PC가 보드를 못 찾습니다. 휴대폰을 PC에 꽂았을 때 파일 전송이 되는 케이블이면 데이터 케이블입니다.
- **와이파이/블루투스 안테나** (보드 상자에 들어 있음): 안테나 없이 쓰면 신호가 매우 약합니다.
- (선택) 점퍼선 1개: D0 핀에 꽂아 두면 터치하기 편합니다.

### 소프트웨어
- [Arduino CLI](https://arduino.github.io/arduino-cli/) (`arduino-cli`)
- (4단계) [Supabase](https://supabase.com) 무료 계정
- (4단계) [GitHub](https://github.com) 계정과 [GitHub CLI](https://cli.github.com/) (`gh`): 웹 대시보드 배포용
- (5단계) [Tailscale](https://tailscale.com) 무료 계정, PC와 스마트폰 앱
- (7단계) 안드로이드 크롬, 또는 [nRF Connect](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile) 앱
- (8단계) [Edge Impulse](https://studio.edgeimpulse.com) 무료 계정, [Python](https://www.python.org/) 3.9 이상

---

## 3. 폴더 구조

```
.
├── README.md                 ← 지금 보고 있는 문서
├── Blink/                    ← 1단계: LED 깜빡이기
├── TouchLED/                 ← 2단계: 터치 → LED
├── WebDashboard/             ← 3단계: 같은 와이파이 웹 대시보드
│   └── secrets.example.h     ←   와이파이 정보 예시 (secrets.h로 복사해서 사용)
├── SupabaseDashboard/        ← 4단계: Supabase 연동 + 내부망 대시보드 + 카메라
│   ├── SupabaseDashboard.ino ←   메인: 터치 읽기, Supabase와 주고받기
│   ├── lan_server.h          ←   내부망 대시보드 웹 서버 + 카메라 스트리밍
│   ├── device_state.h        ←   LED/터치 상태 (메인과 웹 서버가 함께 씀)
│   └── secrets.example.h     ←   와이파이 + Supabase 정보 예시
├── APDashboard/              ← 6단계: AP 모드
├── BLEControl/               ← 7단계: BLE
├── EICollector/              ← 8단계: 사진 수집 대시보드 (카메라 + 웹 서버)
│   ├── EICollector.ino       ←   카메라, 사진 찍기/설정 API, 스트리밍
│   ├── collector_page.h      ←   수집 대시보드 화면 (HTML/JS)
│   └── secrets.example.h     ←   와이파이 정보 예시
├── dataset/                  ← 8단계: 모은 사진 (cup/, phone/)
├── ei_library/               ← 8단계: 학습된 모델 (Arduino 라이브러리 ZIP)
├── web/
│   ├── index.html            ← 외부망 대시보드 (Supabase)
│   └── ble.html              ← BLE 대시보드 (웹 블루투스)
├── supabase/schema.sql       ← Supabase 테이블 만드는 SQL
├── tools/ble_check.py        ← PC에서 BLE 동작 확인하는 스크립트
├── tools/ei_*.py             ← Edge Impulse 업로드·학습·라이브러리 받기 스크립트
├── index.html, ble.html      ← 예전 주소를 web/ 으로 옮겨 주는 안내 페이지
└── .gitignore                ← 비밀번호 파일이 올라가지 않게 막는 목록
```

**`secrets.h`가 없는 이유**: 와이파이 비밀번호가 들어가는 파일이라 GitHub에 올리지 않습니다(`.gitignore`). 직접 만들어야 합니다. 방법은 [4-3](#4-3-와이파이-정보-넣기-secretsh)을 보세요.

---

## 4. 개발 환경 만들기 (arduino-cli)

> 아래 명령은 Windows 기준입니다. PowerShell이나 Git Bash 어디서든 똑같이 입력하면 됩니다.

### 4-1. ESP32 보드 패키지 설치

arduino-cli는 처음엔 아두이노 우노 같은 AVR 보드만 압니다. ESP32를 쓰려면 Espressif가 제공하는 보드 패키지를 추가해야 합니다.

```bash
# 1) 설정 파일 만들기 (처음 한 번만)
arduino-cli config init

# 2) ESP32 보드 목록 주소 추가
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# 3) 보드 목록 새로고침 후 ESP32 패키지 설치 (수 GB, 시간이 꽤 걸립니다)
arduino-cli core update-index
arduino-cli core install esp32:esp32

# 4) 설치 확인: esp32:esp32 가 보이면 성공
arduino-cli core list
```

> 💾 **디스크 공간 주의**: ESP32 패키지는 지원하는 모든 칩(C3, C6, S2, S3, P4…)의 라이브러리를 한꺼번에 설치해서 **5~10GB**를 씁니다. C 드라이브가 부족하면 설치 도중 `There is not enough space on the disk` 오류가 납니다. 아래처럼 D 드라이브에 설치할 수 있습니다.
>
> `C:\Users\<사용자>\AppData\Local\Arduino15\arduino-cli.yaml` 파일에 다음을 넣으면 됩니다.
> ```yaml
> directories:
>     data: D:\Arduino15
>     downloads: D:\Arduino15\staging
> ```
> 폴더를 옮긴 뒤 원래 경로에 바로가기 링크(정션)를 만드는 방식은 arduino-cli가 제대로 따라가지 못해서 동작하지 않았습니다. 위처럼 설정 파일에서 경로를 바꾸세요.

### 4-2. 보드 포트(COM 번호) 찾기

보드를 USB-C 데이터 케이블로 PC에 꽂고:

```bash
arduino-cli board list
```

```
Port Protocol Type              Board Name          FQBN ...
COM4 serial   Serial Port (USB) ESP32 Family Device ...
COM8 serial   Serial Port       Unknown
```

- `Serial Port (USB)`로 표시되는 것이 보드입니다. 위 예에서는 **COM4**입니다.
- `Serial Port`만 있고 `(USB)`가 없는 항목은 대개 **블루투스 가상 포트**라 보드가 아닙니다.
- 새 포트가 아예 안 생기면 → [7. 문제 해결](#보드를-꽂아도-포트가-안-보여요)을 보세요.

이 문서의 명령은 모두 `COM4` 기준입니다. 본인 포트 번호로 바꿔서 입력하세요.

### 4-3. 와이파이 정보 넣기 (`secrets.h`)

3·4단계(`WebDashboard`, `SupabaseDashboard`)는 와이파이 정보가 필요합니다.

1. 폴더 안의 `secrets.example.h`를 복사해서 이름을 `secrets.h`로 바꿉니다.
2. `your-wifi-name`, `your-wifi-password`를 내 와이파이 정보로 바꿉니다.

> 📶 **ESP32-S3는 2.4GHz 와이파이만 지원합니다.** 공유기가 `MyWifi`와 `MyWifi_5G`처럼 두 개를 띄우면, `_5G`가 **아닌** 쪽을 넣으세요.

### 4-4. 업로드하는 기본 명령

```bash
arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3 <폴더이름>
```

- `compile`: 코드를 보드가 이해하는 기계어로 바꿉니다.
- `--upload -p COM4`: 바꾼 결과를 COM4에 연결된 보드에 올립니다.
- `--fqbn esp32:esp32:XIAO_ESP32S3`: 보드 종류(XIAO ESP32-S3)를 알려줍니다.
- 마지막 줄에 `Hard resetting via RTS pin...`이 나오면 업로드 성공입니다.

> 📷 **카메라를 쓰는 4단계는 PSRAM 옵션이 필요합니다**: `--fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi`

### 4-5. 시리얼 모니터로 보드 로그 보기

```bash
arduino-cli monitor -p COM4 -c baudrate=115200
```

보드가 `Serial.println()`으로 출력하는 메시지(IP 주소, 오류 등)를 볼 수 있습니다. 끝낼 때는 `Ctrl + C`를 누르세요.

> ⚠️ 시리얼 모니터가 켜져 있으면 COM4를 쓰고 있어서 **업로드가 실패**합니다. 업로드 전에 꼭 닫으세요.

---

## 5. 먼저 알아두면 좋은 개념

### 보드의 버튼 두 개 (R, B)
USB-C 포트 옆에 아주 작은 버튼 두 개가 있습니다.

| 버튼 | 이름 | 하는 일 |
|---|---|---|
| **R** | Reset | 누르면 프로그램이 처음부터 다시 실행됩니다. 전원을 뺐다 꽂는 것과 같습니다. |
| **B** | Boot | **B를 누른 채로** R을 누르거나 USB를 꽂으면 "업로드 대기 모드(부트로더)"로 들어갑니다. 업로드가 계속 실패할 때 씁니다. |

B만 눌러서는 아무 일도 일어나지 않으니 걱정하지 않아도 됩니다. 부트로더 모드에 들어갔다면 업로드한 뒤 **R을 한 번** 누르면 다시 정상 실행됩니다.

### LED는 "거꾸로" 동작합니다 (active-low)
보드의 주황색 USER LED(GPIO21)는 **LOW를 주면 켜지고, HIGH를 주면 꺼집니다.** 처음 보면 헷갈리는 부분입니다.

```cpp
digitalWrite(LED_BUILTIN, LOW);   // 켜짐
digitalWrite(LED_BUILTIN, HIGH);  // 꺼짐
```

### 터치 센서는 이렇게 판단합니다
ESP32-S3는 핀 몇 개에 정전식 터치 센서가 들어 있습니다(D0 = GPIO1 = TOUCH1). `touchRead(D0)`는 숫자를 돌려주는데, **손을 대면 숫자가 커집니다.** (구형 ESP32와 반대입니다.)

이 프로젝트의 판단 방식:
1. **기준값 측정**: 켜질 때 손을 안 댄 상태의 값을 여러 번 재서 평균을 냅니다(예: 17,590).
2. **처음 값은 버리기**: 센서가 켜진 직후 50번 정도는 값이 비정상적으로 크게(10배 이상) 나옵니다. 이 값으로 기준을 잡으면 터치가 영영 인식되지 않아서, 처음 50번은 버립니다.
3. **판단**: 현재 값이 `기준값 × 1.1`을 넘으면 터치로 봅니다. 숫자를 낮추면(1.05) 더 민감하고, 높이면(1.3) 덜 민감합니다.
4. **기준값 자동 보정**: 손을 안 댄 동안에는 기준값을 조금씩 현재 값 쪽으로 맞춥니다. 온도나 습도로 값이 천천히 변해도 오작동하지 않습니다.

> 👉 보드를 켜거나 리셋할 때 **D0 핀에 손을 대지 마세요.** 기준값이 잘못 잡힙니다.

---

## 6. 단계별 실습

### 6-1. Blink: LED 깜빡이기

**무엇을 하나요?** 보드의 주황색 LED가 0.1초 간격으로 깜빡입니다. 보드와 개발 환경이 제대로 동작하는지 확인하는 첫 단계입니다.

```bash
arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3 Blink
```

**속도 바꾸기**: `Blink/Blink.ino`의 `delay(100)`이 0.1초(100밀리초)입니다. `delay(1000)`으로 바꾸면 1초 간격이 됩니다.

---

### 6-2. TouchLED: 터치하면 LED 켜기

**무엇을 하나요?** D0 핀을 손가락으로 만지면 LED가 켜지고, 떼면 꺼집니다.

```bash
arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3 TouchLED
```

**사용법**
1. 업로드가 끝나면 **R 버튼을 한 번** 누릅니다. 이때 D0에 손을 대지 마세요.
2. D0 핀(또는 꽂아 둔 점퍼선 끝)을 만집니다.

**값 확인**: 시리얼 모니터(4-5)를 열면 `baseline=17590 value=17592 touched=0` 같은 값이 계속 출력됩니다. 만지면 `value`가 커지고 `touched=1`이 됩니다.

**감도 조절**: `TouchLED.ino`의 `TOUCH_RATIO = 1.1`을 바꾸세요.

---

### 6-3. WebDashboard: 같은 와이파이에서 웹으로 제어

**무엇을 하나요?** 보드가 집 와이파이에 연결해서 작은 웹 서버를 엽니다. 같은 와이파이에 있는 PC나 휴대폰 브라우저에서 LED를 켜고 끄고, 터치 상태를 볼 수 있습니다.

```
[휴대폰/PC 브라우저] ──같은 와이파이──▶ [보드 웹 서버 :80]
```

**준비**: `WebDashboard/secrets.example.h` → `secrets.h`로 복사하고 와이파이 정보 입력 (4-3 참고)

```bash
arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3 WebDashboard
```

**사용법**
1. 시리얼 모니터에서 `IP=192.168.0.23` 같은 주소를 확인합니다.
2. 같은 와이파이에 연결된 브라우저에서 `http://<그 IP>/` 또는 `http://xiao.local/`을 엽니다. (`xiao.local`은 안드로이드에서 안 될 수 있으니 그때는 IP를 쓰세요.)

**구조**

| 주소 | 하는 일 |
|---|---|
| `GET /` | 대시보드 페이지 |
| `GET /api/status` | `{"led":false,"touched":false,"value":17590,...}` JSON 상태 |
| `POST /api/led?on=1` | LED 켜기 (`on=0`이면 끄기) |

페이지는 0.2초마다 `/api/status`를 불러서 화면을 새로 그립니다.

---

### 6-4. SupabaseDashboard: 어디서나 제어 + 내부망 대시보드 + 카메라

**무엇을 하나요?** 이 펌웨어 하나로 대시보드가 **두 개** 생깁니다.

| | 외부망 대시보드 | 내부망 대시보드 |
|---|---|---|
| 주소 | https://worksenddl2-source.github.io/xiao-dashboard/ | `http://<보드 IP>/` |
| 어디서 | 인터넷 되는 곳 어디서나 | 같은 와이파이 (또는 Tailscale, 6-5) |
| LED / 터치 | ✔ | ✔ |
| 카메라 | 내부망 대시보드로 가는 링크 | ✔ 실시간 영상 + 스냅샷 |
| 인터넷 끊기면 | ✗ | ✔ 그대로 동작 |

두 대시보드는 **LED 상태를 공유**합니다. 한쪽에서 켜면 다른 쪽에도 켜진 것으로 보입니다.

#### 어떻게 동작하나요?

```
[외부 브라우저] ⇄ [Supabase DB + Realtime] ⇄ [보드] ⇄ [같은 와이파이 브라우저]
   (GitHub Pages)        (클라우드)          (집)        (내부망 대시보드)
```

- 보드는 Supabase에 **밖으로 나가는 요청만** 보냅니다. 그래서 공유기 포트포워딩 설정이 필요 없습니다.
  - 0.5초마다 LED 명령을 읽습니다(GET).
  - 터치 상태가 바뀔 때와 5초마다 상태를 올립니다(PATCH).
- 외부망 대시보드는 Supabase **Realtime**으로 값이 바뀌는 즉시 화면을 갱신합니다.
- 보드가 15초 넘게 보고하지 않으면 외부망 대시보드에 "보드 오프라인"이 표시됩니다.

#### 준비 1: Supabase 테이블 만들기

1. https://supabase.com 에 가입하고 프로젝트를 만듭니다. 리전은 서울(`ap-northeast-2`)을 추천합니다.
2. 왼쪽 메뉴 **SQL Editor**에서 [`supabase/schema.sql`](supabase/schema.sql) 내용을 전부 붙여넣고 **Run**을 누릅니다.
3. **Project Settings → API**에서 두 값을 복사합니다.
   - Project URL: `https://xxxx.supabase.co`
   - **Publishable key**: `sb_publishable_...` (⚠️ `secret` / `service_role` 키는 절대 쓰지 마세요)

`schema.sql`이 만드는 것:
- `xiao_device` 테이블 한 개, 행 한 개(`id = 'xiao'`)
- 공개 키로는 **읽기와 값 수정만** 됩니다. 행 추가와 삭제는 막혀 있습니다.
- Realtime 켜기, 마지막 보고 시각을 자동으로 기록하는 트리거

#### 준비 2: `secrets.h`
`SupabaseDashboard/secrets.example.h` → `secrets.h`로 복사하고 와이파이, Supabase URL, 키를 넣습니다.

#### 업로드 (PSRAM 옵션 필수)

```bash
arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi SupabaseDashboard
```

시리얼 모니터에 이렇게 나오면 성공입니다.
```
IP: 10.22.149.227
Camera: sensor PID 0x3660 ready      ← 카메라 인식 (0x3660 = OV3660, 0x5640 = OV5640)
```
카메라가 없는 일반 XIAO ESP32-S3에서는 `Camera: init failed`가 나오지만, LED와 터치는 정상 동작합니다.

#### 내부망 대시보드 기능 (`http://<보드 IP>/`)

| 주소 | 하는 일 |
|---|---|
| `/` | 카메라 + LED + 터치 대시보드 |
| `/api/status` | JSON 상태 |
| `POST /api/led?on=1` | LED 켜기/끄기 (Supabase에도 반영) |
| `/capture` | 지금 화면 한 장 (JPEG) |
| `:81/stream` | 실시간 영상 (MJPEG, 640×480, 약 20fps) |

- 영상 전송은 81번 포트의 별도 서버에서 처리합니다. 그래서 영상이 나오는 동안에도 LED 버튼이 바로 반응합니다.
- **스트림은 한 번에 한 명만** 볼 수 있습니다. 두 번째 사람에게는 안내 문구가 나옵니다.
- 카메라를 오래 쓰면 보드가 뜨거워집니다. 방열판 사용을 권장합니다.

#### 외부망 대시보드 배포 (GitHub Pages)

`web/index.html`은 서버가 필요 없는 정적 페이지라 GitHub Pages로 무료 배포할 수 있습니다.

```bash
# 저장소 만들고 올리기
gh repo create <내아이디>/xiao-dashboard --public --source . --push

# GitHub Pages 켜기 (main 브랜치 루트)
gh api -X POST repos/<내아이디>/xiao-dashboard/pages -f "source[branch]=main" -f "source[path]=/"
```

1~2분 뒤 `https://<내아이디>.github.io/xiao-dashboard/`에서 열립니다. 내 Supabase를 쓰려면 `web/index.html` 위쪽의 `SUPABASE_URL`, `SUPABASE_KEY`를 바꾸세요.

> ❓ **외부망 대시보드 안에 카메라 영상을 바로 넣지 않은 이유**: 외부망 대시보드는 `https://`, 보드 카메라는 `http://`(암호화 없음)입니다. 브라우저는 https 페이지 안에 http 영상이 섞이는 것("혼합 콘텐츠")을 차단합니다. 그래서 내부망 대시보드로 가는 **링크**만 넣었습니다.

---

### 6-5. Tailscale: 밖에서 내부망 대시보드·카메라 보기

**무엇을 하나요?** 집 밖(모바일 데이터 등)에서도 `http://<보드 IP>/` 내부망 대시보드와 **실시간 카메라**를 볼 수 있게 합니다.

ESP32에는 Tailscale을 설치할 수 없습니다. 그래서 **같은 와이파이에 항상 켜져 있는 PC 한 대**가 중계 역할(서브넷 라우터)을 합니다.

```
[밖에 있는 휴대폰 + Tailscale 앱] ──Tailscale──▶ [집 PC (서브넷 라우터)] ──와이파이──▶ [보드]
```

#### 설정 순서

1. **PC에 Tailscale 프로그램 설치**
   ```bash
   winget install --id Tailscale.Tailscale -e
   ```
   > ⚠️ 크롬에서 tailscale.com을 "앱으로 설치"한 것은 **웹사이트 바로가기**일 뿐 실제 프로그램이 아닙니다. 그걸로는 동작하지 않습니다. 설치 후 `C:\Program Files\Tailscale\tailscale.exe`가 있는지 확인하세요.

2. **로그인하고 보드 IP 하나만 공개하기**
   ```bash
   "C:\Program Files\Tailscale\tailscale.exe" up --advertise-routes=10.22.149.227/32
   ```
   - 로그인 링크가 나오면 브라우저에서 로그인합니다.
   - `10.22.149.227`은 내 보드 IP로 바꾸세요. `/32`는 "이 주소 하나만"이라는 뜻입니다. 같은 와이파이의 다른 기기는 공개되지 않습니다.

3. **관리 콘솔에서 승인하기** (꼭 필요)
   - https://login.tailscale.com/admin/machines 접속
   - PC 오른쪽 `...` → **Edit route settings** → 보드 IP 체크 → 저장

4. **휴대폰에 Tailscale 앱 설치**하고 같은 계정으로 로그인합니다.

5. **테스트**: 휴대폰 **와이파이를 끄고** 모바일 데이터 + Tailscale 켠 상태에서 `http://10.22.149.227/` 접속

#### 승인됐는지 확인하는 방법
```bash
"C:\Program Files\Tailscale\tailscale.exe" status --json
```
결과의 `Self` 안 `PrimaryRoutes`에 보드 IP가 보이면 승인된 것입니다.

#### 주의
- 중계하는 **PC가 켜져 있고, 보드와 같은 와이파이에 연결돼 있어야** 합니다. PC가 절전 모드로 들어가거나 다른 와이파이로 바뀌면 끊깁니다.
- 공유기가 보드 IP를 바꾸면 경로를 다시 설정해야 합니다. 공유기 설정에서 보드 IP를 **고정(DHCP 예약)** 해 두세요.
- 휴대폰 Tailscale 앱에서 **"Use Tailscale subnets"** 옵션이 켜져 있어야 합니다.

---

### 6-6. APDashboard: 보드가 직접 와이파이를 뿌리기

**무엇을 하나요?** 공유기 없이 **보드가 직접 와이파이(AP)를 만듭니다.** 휴대폰을 그 와이파이에 연결하면 보드가 띄운 웹 페이지에서 LED와 터치를 제어할 수 있습니다. 인터넷이 전혀 없는 곳에서도 됩니다.

```
[휴대폰] ──"XIAO-ESP32S3" 와이파이──▶ [보드 = 공유기 + 웹 서버 (192.168.4.1)]
```

```bash
arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3 APDashboard
```

**사용법**
1. 휴대폰 와이파이 목록에서 **`XIAO-ESP32S3`** 선택, 비밀번호 **`xiao1234`**
2. "인터넷에 연결되어 있지 않음" 알림이 뜨면 **"연결 유지"** 를 선택하세요. 안 그러면 휴대폰이 모바일 데이터로 넘어가서 페이지가 안 열립니다.
3. 브라우저에서 **http://192.168.4.1** 접속

**화면 구성**: LED 켜기/끄기, 터치 상태와 측정값 숫자, 막대 그래프, 현재 접속한 기기 수

**와이파이 이름과 비밀번호 바꾸기**: `APDashboard.ino`의 `AP_SSID`, `AP_PASS`를 바꾸세요. 비밀번호는 **8자 이상**이어야 합니다.

> 인터넷이 없는 환경이라 페이지에 외부 글꼴이나 라이브러리를 쓰지 않고, 보드 안에 있는 코드만 씁니다.

---

### 6-7. BLEControl: 스마트폰과 블루투스로 직접 연결

**무엇을 하나요?** 와이파이 없이 **블루투스 저전력(BLE)** 으로 스마트폰과 보드를 직접 연결해서 LED를 제어하고 터치값을 봅니다.

> ESP32-S3는 **BLE만 지원**하고, 옛날 방식 블루투스(Classic, 예: `BluetoothSerial`)는 지원하지 않습니다.

```bash
arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3 BLEControl
```

#### BLE 기본 용어 (아주 짧게)
- **서버(Server)**: 데이터를 가진 쪽입니다. 여기서는 보드입니다.
- **클라이언트(Client)**: 데이터를 읽고 쓰는 쪽입니다. 여기서는 스마트폰입니다.
- **서비스(Service)**: 관련 기능을 묶은 그룹입니다.
- **특성(Characteristic)**: 실제로 읽고 쓰는 값 하나입니다.
  - **Read**: 읽기
  - **Write**: 쓰기
  - **Notify**: 값이 바뀌면 보드가 먼저 알려줌
- **UUID**: 서비스와 특성을 구분하는 고유 번호입니다.

#### 이 프로젝트의 BLE 구성

| 항목 | UUID | 속성 | 값 |
|---|---|---|---|
| 기기 이름 | — | — | `XIAO_ESP32S3` |
| 서비스 | `7f1c0001-5a3e-4c8b-9d2a-1b6e4f0a9c11` | — | — |
| 터치 | `7f1c0002-5a3e-4c8b-9d2a-1b6e4f0a9c11` | Read, Notify | `측정값,터치여부,기준값` 예: `17672,0,17664` (0.2초마다 알림) |
| LED | `7f1c0003-5a3e-4c8b-9d2a-1b6e4f0a9c11` | Read, Write, Notify | `1` = 켜기, `0` = 끄기 |

#### 방법 A: 웹 블루투스 대시보드 (추천, 안드로이드)

1. 휴대폰 **블루투스를 켜고** 안드로이드 **크롬**으로 접속합니다.
   **https://worksenddl2-source.github.io/xiao-dashboard/web/ble.html**
2. **"블루투스로 연결"** → 목록에서 **XIAO_ESP32S3** 선택
   - "근처 기기" 권한을 물으면 허용하세요. 위치 서비스가 켜져 있어야 할 수도 있습니다.
3. 연결되면 LED 버튼과 터치 상태가 실시간으로 동작합니다.

- 웹 블루투스는 **https 주소에서만** 동작합니다. 그래서 GitHub Pages에 올렸습니다.
- **아이폰 사파리는 웹 블루투스를 지원하지 않습니다.** 아이폰에서는 Bluefy 앱 브라우저를 쓰거나 방법 B를 쓰세요.
- PC의 크롬이나 엣지에서도 됩니다.

#### 방법 B: nRF Connect 앱

1. 앱에서 **SCAN** → `XIAO_ESP32S3` → **CONNECT**
2. 서비스 `7f1c0001-…`을 펼칩니다.
3. **터치값**: `7f1c0002-…`의 알림 버튼(↓↓↓)을 누르면 값이 계속 들어옵니다.
4. **LED**: `7f1c0003-…`의 쓰기 버튼(↑)을 누르고 형식을 **Text(UTF-8)** 로 바꾼 뒤 `1` 또는 `0`을 보냅니다.

#### PC에서 동작 확인하기 (선택)
블루투스가 있는 PC라면 스크립트로 확인할 수 있습니다.
```bash
python -m pip install bleak
python tools/ble_check.py
```
```
found E8:F6:0A:8B:E4:8D
touch read: 17672,0,17664
touch notifications in 2 s: 10, last = 17677,0,17664
LED after writing 1: 1
LED after writing 0: 0
```

#### 코드에서 눈여겨볼 점
- **연결이 끊기면 광고(advertising)를 다시 시작**해야 합니다. 안 그러면 한 번 끊긴 뒤 다시 검색되지 않습니다(`onDisconnect`).
- Notify를 쓰려면 특성에 `BLE2902` 디스크립터를 붙여야 합니다.
- ESP32 보드 패키지 **3.x 버전**에서는 `getValue()`가 `std::string`이 아니라 Arduino `String`을 돌려줍니다. 인터넷의 옛 예제를 쓸 때 컴파일 오류가 나면 이 부분을 확인하세요.

---

### 6-8. EICollector + Edge Impulse: 카메라 사진으로 물체 분류 모델 만들기

카메라로 **컵(cup)** 과 **휴대폰(phone)** 사진을 모으고, [Edge Impulse](https://edgeimpulse.com)에서 "이 사진이 컵인지 휴대폰인지" 맞히는 모델을 학습시킵니다. 학습한 모델은 XIAO에서 바로 돌릴 수 있는 Arduino 라이브러리로 받아 둡니다(보드에서 추론하는 코드는 다음 단계).

전체 흐름은 이렇습니다.

```
① EICollector 펌웨어  →  ② 브라우저에서 사진 찍기  →  ③ ZIP 다운로드 → dataset/
   (카메라 + 웹 서버)        (클래스별로 저장)

④ tools/ei_upload.py  →  ⑤ tools/ei_train.py  →  ⑥ tools/ei_deploy.py
   (Edge Impulse에 올리기)  (학습 + 정확도 확인)     (Arduino 라이브러리 받기)
```

#### 1) 수집용 펌웨어 올리기

`EICollector/secrets.example.h`를 `secrets.h`로 복사해 와이파이 정보를 넣고([4-3](#4-3-와이파이-정보-넣기-secretsh)), **PSRAM 옵션을 붙여서** 올립니다.

```bash
arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi EICollector
```

시리얼 모니터에 `Collector: http://10.22.149.227` 처럼 주소가 10초마다 찍힙니다. **같은 와이파이**에 있는 PC나 휴대폰 브라우저로 그 주소(또는 `http://xiao.local`)를 엽니다.

#### 2) 사진 모으기 (수집 대시보드)

| 기능 | 하는 법 |
|---|---|
| 클래스 만들기 | 오른쪽 위 입력칸에 `cup`, `phone` 처럼 이름을 넣고 **추가** |
| 클래스 고르기 | 클래스 줄을 누르거나 숫자키 `1`~`9`. 화면 왼쪽 위에 지금 클래스가 표시됨 |
| 한 장 찍기 | **찍기** 버튼 또는 `Space` |
| 방금 찍은 것 지우기 | **되돌리기** 또는 `Z` |
| 연속 촬영 | 장수·간격(ms)·시작 전 대기(3/5/10초)를 정하고 **연속 촬영 시작** 또는 `B` (다시 누르면 중지) |
| 잘못 찍은 사진 정리 | 사진을 누르면 크게 보임 → `←` `→`로 넘기며 `Del`로 삭제. 체크박스로 여러 장 골라 **선택 삭제** / 다른 클래스로 **이동** |
| 내려받기 | 클래스 줄의 **ZIP**(그 클래스만) 또는 **전체 ZIP**(클래스별 폴더) |
| 카메라 설정 | 해상도(기본 240x240), JPEG 품질, 밝기, 대비, 상하/좌우 반전 |

- 사진은 **보드가 아니라 브라우저(IndexedDB)에 저장**됩니다. 새로고침해도 남지만, 다른 PC·브라우저와는 공유되지 않으니 ZIP으로 받아 두세요.
- 파일 이름이 `cup.0001.jpg` 형식이라 Edge Impulse가 이름에서 라벨(cup)을 알아냅니다.
- 찍을 때마다 보드의 주황 LED가 깜빡입니다.
- 연속 촬영 중에는 물체를 **조금씩 돌리고, 위치와 거리를 바꿔** 주세요. 똑같은 사진만 많으면 모델이 그 장면만 외웁니다.

받은 ZIP은 `dataset/<클래스>/`에 풉니다. 이 저장소에는 cup 100장, phone 100장(240x240)이 들어 있습니다.

```
dataset/
├── cup/     cup.0001.jpg ~ cup.0100.jpg
└── phone/   phone.0001.jpg ~ phone.0100.jpg
```

#### 3) Edge Impulse API 키 준비

1. [studio.edgeimpulse.com](https://studio.edgeimpulse.com)에서 무료 계정을 만들고 프로젝트를 하나 만듭니다.
2. 프로젝트의 **Dashboard → Keys** 에서 API 키(`ei_...`)를 복사합니다.
3. 저장소 맨 위 폴더에 `.env` 파일을 만들고 한 줄 넣습니다. (`.env`는 `.gitignore`에 있어서 GitHub에 올라가지 않습니다.)

```
EI_API_KEY=ei_여기에_복사한_키
```

#### 4) 업로드 → 학습 → 라이브러리 받기

스크립트는 파이썬 기본 기능만 써서 따로 설치할 것이 없습니다(Python 3.9 이상).

```bash
python tools/ei_upload.py   # dataset/ 사진을 80% 학습용, 20% 테스트용으로 나눠 올림
python tools/ei_train.py    # 특징 추출 → 학습 → 테스트 사진으로 정확도 확인
python tools/ei_deploy.py   # ESP32용 Arduino 라이브러리(int8)를 ei_library/에 저장
```

> ⚠️ `ei_upload.py`는 처음 한 번만 실행하세요. 사진을 더 찍었다면 새 사진만 따로 올리는 것이 안전합니다(같은 사진이 두 번 들어갈 수 있음).

`ei_train.py`가 만드는 모델(Edge Impulse에서는 "임펄스"라고 부름)은 이렇습니다.

| 단계 | 설정 | 이유 |
|---|---|---|
| 입력 | 사진을 96x96으로 줄임, 컬러(RGB) | 작을수록 보드에서 빠름 |
| 학습 | **MobileNetV1 0.25** 전이학습, 60회 반복, 데이터 증강(뒤집기·자르기·밝기) | 이미 학습된 작은 모델을 재활용해 적은 사진으로도 잘 맞힘 |
| 양자화 | **int8** | 소수(float32) 대신 정수로 계산해 ESP32-S3에서 훨씬 빠르고 작음 |

다른 모델로 학습해 보려면 이름과 반복 횟수를 넘깁니다: `python tools/ei_train.py transfer_mobilenetv2_a35 40`

#### 결과

테스트용 사진 40장(학습에 쓰지 않은 사진)으로 확인한 정확도입니다.

| 모델 | float32 | int8 (보드에서 쓸 것) |
|---|---|---|
| MobileNetV2 0.35 (Edge Impulse 기본값) | 100% | **80%** (phone 4장을 cup으로 착각) |
| **MobileNetV1 0.25 (채택)** | 100% | **100%** |

기본값인 MobileNetV2는 int8로 줄이자 정확도가 크게 떨어져서, 양자화에 강한 MobileNetV1으로 바꿨습니다. 결과 라이브러리는 `ei_library/ei-arduino-library.zip`(이름 `worksenddl2-source-project-1_inferencing`)입니다.

#### 알아둘 점
- 사진을 한 장소·한 조명에서 찍었기 때문에 **100%는 낙관적인 숫자**입니다. 다른 책상, 다른 조명에서도 찍어 섞어 주면 실제 정확도가 올라갑니다.
- 지금 모델은 "컵 또는 휴대폰" 두 가지만 압니다. **아무것도 없는 화면에서도 둘 중 하나로 대답**합니다. 물체가 없는 배경 사진을 `background` 클래스로 모아 함께 학습시키면 이 문제가 줄어듭니다.
- 학습할 때와 보드에서 추론할 때 **카메라 설정(해상도·밝기·반전)을 똑같이** 맞추세요.

---

## 7. 문제 해결 (FAQ)

#### 보드를 꽂아도 포트가 안 보여요
1. **케이블이 충전 전용인지** 의심하세요. 가장 흔한 원인입니다. 다른 USB-C 케이블로 바꿔 보세요.
2. **부트로더 모드**로 연결해 보세요. **B 버튼을 누른 채로** USB를 꽂고, 꽂은 뒤에 B를 뗍니다.
3. 보드가 연결되면 장치 관리자에 `USB 직렬 장치 (COMx)`가 생기고, ID는 `VID_303A`(Espressif)입니다.

#### 업로드가 실패해요
- 시리얼 모니터나 다른 프로그램이 COM 포트를 잡고 있지 않은지 확인하세요.
- 위의 부트로더 모드로 넣은 뒤 다시 업로드하고, 끝나면 **R 버튼**을 누르세요.

#### 업로드는 됐는데 시리얼 모니터에 아무것도 안 나와요
- XIAO ESP32-S3는 기본 설정(`USB CDC On Boot = Enabled`)에서만 USB로 시리얼이 나옵니다. `--fqbn ...:CDCOnBoot=cdc`를 붙이면 이 보드에서는 오히려 **꺼집니다**. FQBN에 아무 옵션도 붙이지 마세요(카메라용 `PSRAM=opi`는 괜찮습니다).
- 보드가 부트로더 모드에 머물러 있을 수 있으니 **R 버튼**을 눌러 보세요.

#### 터치해도 반응이 없어요
- 켜질 때 D0에 손을 대고 있었다면 기준값이 잘못 잡힙니다. 손을 떼고 **R 버튼**을 누르세요.
- 시리얼 모니터에서 `baseline`과 `value`를 비교해 보세요. 손을 안 댄 상태에서 둘이 비슷해야 정상입니다.
- 감도를 높이려면 `TOUCH_RATIO`를 1.05 정도로 낮추세요.

#### 와이파이에 연결이 안 돼요 (`Connecting to Wi-Fi.....`가 계속 나옴)
- **2.4GHz** 와이파이인지 확인하세요. 5GHz는 지원하지 않습니다.
- `secrets.h`의 이름과 비밀번호에 오타가 없는지 확인하세요. 대소문자와 특수문자까지 정확해야 합니다.
- **안테나**가 꽂혀 있는지 확인하세요.

#### `ESP32 코어 설치 중 not enough space on the disk`
C 드라이브 공간이 부족합니다. [4-1의 D 드라이브 설치 방법](#4-1-esp32-보드-패키지-설치)을 보세요.

#### 카메라가 `init failed`라고 나와요
- **Sense 확장보드(카메라)** 가 꽂혀 있는지 확인하세요. 일반 XIAO ESP32-S3에는 카메라가 없습니다.
- FQBN에 `:PSRAM=opi`를 붙였는지 확인하세요.

#### 카메라 영상이 옆으로 누워 있어요
보드를 놓은 방향 때문입니다. `lan_server.h`의 `s->set_vflip(s, 1);` 근처에 `s->set_hmirror(s, 1);`을 추가하는 등으로 방향을 바꿀 수 있습니다. 90도 회전은 센서에서 지원하지 않아서, 보드 놓는 방향을 바꾸는 게 가장 간단합니다.

#### 외부망 대시보드가 "보드 오프라인"이에요
- 보드에 `SupabaseDashboard`가 올라가 있는지 확인하세요. 다른 펌웨어를 올리면 Supabase 보고가 멈춥니다.
- 시리얼 모니터에 `PATCH failed` 같은 오류가 나오는지 확인하세요.

#### 웹 블루투스 버튼을 눌러도 기기가 안 보여요
- 보드에 `BLEControl`이 올라가 있는지 확인하세요.
- 휴대폰의 블루투스, 위치 서비스, 크롬의 "근처 기기" 권한을 확인하세요.
- 다른 앱(nRF Connect 등)이 이미 보드에 연결돼 있으면 검색되지 않습니다. 그 앱의 연결을 끊으세요.

---

## 8. 보안 주의사항

- **`secrets.h`, `WIFI.md`, `.env`(Edge Impulse API 키)는 절대 GitHub에 올리지 마세요.** `.gitignore`에 들어 있어서 실수로 올라가지 않습니다.
- Supabase **Publishable key**는 원래 공개해도 되는 키입니다. 웹 페이지에 들어가 있습니다. 반대로 **secret / service_role 키**는 데이터베이스 전체 권한이 있으니 코드나 웹에 절대 넣지 마세요.
- 이 프로젝트의 외부망 대시보드는 **주소를 아는 사람 누구나 LED를 조작할 수 있게** 일부러 열어 두었습니다. 다만 행 추가와 삭제는 막혀 있습니다. 중요한 장치를 제어할 때는 Supabase 로그인(Auth)을 붙이세요.
- 보드는 Supabase에 접속할 때 서버 인증서를 검사하지 않습니다(`setInsecure()`). 통신은 암호화되지만, 누군가 중간에서 서버를 사칭하면 막지 못합니다. LED 실습에는 괜찮지만, 실제 제품에는 루트 인증서를 넣으세요.
- AP 모드 기본 비밀번호(`xiao1234`)는 꼭 바꿔서 쓰세요.
- Tailscale은 보드 IP 하나(`/32`)만 공개해서, 같은 와이파이의 다른 기기는 노출되지 않습니다.

---

## 9. 참고 자료

- Seeed Studio 위키
  - [XIAO ESP32-S3 시작하기](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/): 핀 배치, 버튼, 부트로더 모드
  - [XIAO ESP32-S3 블루투스](https://wiki.seeedstudio.com/xiao_esp32s3_bluetooth/): BLE 개념, 서버/클라이언트 예제
  - [XIAO ESP32-S3 카메라](https://wiki.seeedstudio.com/xiao_esp32s3_camera_usage/)
- [Arduino CLI 문서](https://arduino.github.io/arduino-cli/)
- [arduino-esp32 (Espressif)](https://github.com/espressif/arduino-esp32)
- [Supabase 문서](https://supabase.com/docs): REST API, Realtime, Row Level Security
- [Tailscale 서브넷 라우터](https://tailscale.com/kb/1019/subnets)
- [Web Bluetooth API (MDN)](https://developer.mozilla.org/docs/Web/API/Web_Bluetooth_API)
- [Edge Impulse 문서](https://docs.edgeimpulse.com/): 이미지 분류, Ingestion API, Arduino 라이브러리 배포

### 핀 요약 (이 프로젝트에서 쓴 것)

| 용도 | 핀 이름 | GPIO | 비고 |
|---|---|---|---|
| USER LED | — | GPIO21 | LOW = 켜짐 |
| 터치 센서 | D0 | GPIO1 | TOUCH1 |
| BOOT 버튼 | B | GPIO0 | 부트로더 진입 |
| 카메라 (Sense) | — | GPIO10~18, 38~40, 47, 48 | `lan_server.h` 참고 |
