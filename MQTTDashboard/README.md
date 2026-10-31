# MQTTDashboard: 터치 센서 + LED를 MQTT로 (로컬 개발)

```
[XIAO] --TCP 1884--> [Mosquitto (PC)] <--WebSocket 9001-- [브라우저 http://localhost:8080]
```

| 토픽 | 방향 | 내용 |
|---|---|---|
| `xiao/status` | 보드 → | `online` / `offline` (retained, 끊기면 브로커가 `offline` 발행) |
| `xiao/touch` | 보드 → | `{"value":17590,"baseline":17580,"touched":false}` 200ms마다 + 상태 바뀔 때 |
| `xiao/led` | 보드 → | 현재 LED 상태 `on` / `off` (retained) |
| `xiao/led/set` | → 보드 | `on` / `off` / `toggle` |

## 실행 순서

1. **브로커**: `start-broker.bat` 실행 (창을 닫으면 브로커도 꺼짐)
   - Windows 서비스 mosquitto는 `127.0.0.1:1883`만 열려 있어 보드가 접속할 수 없으므로, 이 폴더의 `mosquitto.conf`로 별도 브로커를 띄웁니다 (1884 / 9001, 비밀번호 없음 → 개발용).
2. **설정**: `secrets.example.h` → `secrets.h` 복사 후 Wi-Fi(2.4GHz)와 `MQTT_HOST`(PC의 IPv4, `ipconfig`) 입력
   - 보드와 PC가 **같은 공유기**에 있어야 합니다. ESP32-S3는 5GHz Wi-Fi에 접속할 수 없습니다.
3. **업로드** (라이브러리 최초 1회: `arduino-cli lib install PubSubClient`)
   ```
   arduino-cli compile --upload -p COM4 --fqbn esp32:esp32:XIAO_ESP32S3 MQTTDashboard
   ```
4. **웹**: `start-web.bat` 실행 → http://localhost:8080
   - 브라우저용 `mqtt.min.js`(mqtt@5.10.1)는 `web/` 폴더에 들어 있어 인터넷 없이 동작합니다.

## 명령줄로 확인하기

```
"C:\Program Files\mosquitto\mosquitto_sub.exe" -p 1884 -t "xiao/#" -v
"C:\Program Files\mosquitto\mosquitto_pub.exe" -p 1884 -t xiao/led/set -m toggle
```

## 안 될 때

- 시리얼 모니터에 `Connecting to WiFi.....`만 계속 → SSID/비밀번호, 2.4GHz 여부 확인
- `failed, rc=-2` → `MQTT_HOST` IP가 맞는지, 브로커가 켜져 있는지, 방화벽에서 mosquitto 허용 여부 확인
- 웹에 "브로커 연결 끊김" → `start-broker.bat`이 켜져 있는지 확인
