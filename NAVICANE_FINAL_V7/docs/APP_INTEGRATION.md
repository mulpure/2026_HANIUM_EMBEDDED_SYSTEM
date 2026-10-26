# 앱·펌웨어 연동 규약

## 연결

장치명 `NAVCANE_ESP32`, 서비스 `12345678-1234-1234-1234-1234567890ab`, 특성 `abcd1234-1234-1234-1234-abcdef123456`입니다. 특성은 READ/WRITE/WRITE_NR/NOTIFY입니다. UTF-8 메시지는 줄바꿈으로 끝납니다. 명령은 줄바꿈을 제외해 최대 96바이트입니다.

교체 앱은 연결 → MTU 185 요청 → 서비스 탐색 → CCCD 설정 성공 → `STREAM_ON\n` → `GET_STATUS\n` 순서입니다. MTU 요청 실패나 콜백 시간 초과 시에도 서비스 검색을 진행합니다. 쓰기와 CCCD 상태 코드를 확인하며, 쓰기는 이전 완료 콜백 후 다음 조각을 보냅니다. Android 13 이상의 value 인자 콜백과 새 쓰기 API를 사용합니다.

- **기존 앱 모드**: 한 메시지=한 notify입니다. 작은 MTU에서 긴 메시지를 분할하지 않습니다. `ERROR:MTU_LOW`를 보내고 최대 2초 동안 협상을 기다립니다. 협상이 안 되면 긴 메시지를 건너뛰므로 MTU 185 또는 교체 앱이 필요합니다. `FALL_DETECTED`는 MTU 23에도 들어갑니다.
- **STREAM_ON 모드**: 작은 MTU에서 줄바꿈 프레임을 여러 notify로 보낼 수 있습니다. 앱은 바이트를 모으고 완전한 줄에서만 UTF-8을 디코딩합니다. 여러 줄/한글 중간 경계도 처리합니다. 연결마다 이 모드를 다시 요청합니다.
- 앱→ESP32 CFG는 작은 MTU에서 여러 WRITE로 나눌 수 있습니다. 마지막 줄바꿈에서만 명령을 실행합니다. STREAM_ON 모드에서는 REG_CANCEL 같은 명령도 중간 조각을 REG로 오인하지 않습니다.
- 낙상 이벤트는 일반 메시지보다 우선합니다. 이미 전송 중인 프레임이 있으면 그 프레임을 끝낸 뒤 낙상 프레임을 보내 조립을 깨뜨리지 않습니다. 마지막 낙상 1개를 RAM에서 보류하되 **발생 후 5분 미만**일 때만 전송합니다. 정확히 5분부터는 만료합니다. 단절/CCCD 비활성 상태에서도 매 tick 만료를 검사하며, 새 낙상은 발생 시각을 갱신합니다. 만료는 진동/감지 자체를 취소하지 않습니다. 휴대폰·보호자 수신 확인을 뜻하는 ACK는 아닙니다.
- 연결 세대가 바뀌면 미완성 수신, 이전 명령/알림을 폐기합니다. CFG 대기값과 등록 모드도 새 세션에 넘기지 않습니다. 이미 적용한 활성 사용자 설정과 저장 카드 목록은 유지합니다.

5분 보류 정책은 몇 시간 전 사건을 현재 위치로 전달하는 위험을 줄입니다. 발생 당시 위치·절대시각은 이 펌웨어에 저장하지 않으므로 5분 미만의 지연 사건에도 위치 일치까지 보장하지 않습니다. 전원 재시작 시 보류 기록은 사라집니다.

## RFID

| 명령/응답 | 의미 |
|---|---|
| `CFG,name,obstacleCm,heightCode,vib` | **대기 설정** 보관 |
| `ACK,CFG` | CFG 대기 설정 수신 성공, 현재 사용자 설정 변화 없음 |
| `ERR,CFG,FORMAT` / `ERR,CFG,RANGE` | 형식 / 범위 오류 |
| `REG` | 대기 CFG를 다음 태그 카드에 등록 |
| `REG,WAIT,15` | 15초 등록 대기 |
| `REG,OK,UID,NEW,name` | 신규 저장 후 적용 |
| `REG,OK,UID,UPDATE,name` | 기존 카드 갱신 후 적용 |
| `REG,NOCFG` / `REG,NO_RFID` | 대기 설정 없음 / RFID 준비 안 됨 |
| `REG,FSERR` / `REG,FULL` / `REG,TIMEOUT` | 저장소 오류 / 최대 500명 / 시간 초과 |
| `REG_CANCEL` | 대기 중이면 취소 |
| `REG,CANCEL,reason` | USER, DETACHED, RFID_FAULT, DISCONNECTED, CONFIG_CHANGED |
| `APPLY,UID,name` | 등록 카드 인식, 저장 설정 적용 |
| `UNREG,UID` | 미등록 카드, 기본값 적용 |
| `GET_PROFILE_STORE` | `SENSOR:PROFILE_STORE:READY/FAULT` |

이름은 영문·숫자 1~12자, 장애물 20~200cm, 키 코드 14~19, 진동 0(약)/1(강)입니다. 기본값은 50cm/17/1입니다. 키 코드는 앱 신체 정보의 키를 10으로 나눈 정수입니다. 계단 거리와 키는 같은 값이 아니며 펌웨어의 실측표를 사용합니다.

교체 RFID 화면은 **ACK,CFG만** 설정 성공으로 인정합니다. `ACK,STREAM`, `ACK,STAIR`가 등록 버튼을 열지 않습니다. 설정 응답 5초, REG 시작 응답 5초, WAIT 후 대기시간+5초 타이머를 사용합니다. 화면 종료 시 진행 중인 등록을 취소하고 자신만 수신자 목록에서 제거합니다.

`BleManager.listener`는 기존 주 수신자 API입니다. 추가 화면은 `addListener(this)` / `removeListener(this)`를 사용합니다. 다른 파일에서 주 수신자를 계속 덮어쓰는 부분이 있다면 동일하게 바꿔야 합니다. 보호자 연락 처리는 기존 앱의 담당 코드가 수행하며 이 세 파일이 새 연락 기능을 구현하지 않습니다.

## 감지와 상태

| 메시지 | 앱 표시/의미 |
|---|---|
| `FALL_DETECTED` | 낙상, 기존 앱의 보호자 전달 처리 |
| `계단 감지` / `STAIR_CLEARED` | 계단 위험 감지 / 해제, 상태 전환마다 한 이벤트 |
| `OBSTACLE:LEFT/RIGHT/BOTH/NONE` | 하부 좌우 장애물 |
| `OBSTACLE:UPPER/UPPER_CLEAR` | 상부 장애물 / 해제 |
| `포고핀 분리상태` / `포고핀 재결합` | 센서 응답으로 추정한 분절 분리 / 재결합, 개별 센서 고장과 별개 |
| `SENSOR:MPU:READY/FAULT` | 가속도 사용 가능 여부 |
| `SENSOR:STAIR:READY/FAULT` | 계단 사용 가능 여부 |
| `SENSOR:OBSTACLE:READY/FAULT` | 좌우 ToF 사용 가능 여부 |
| `SENSOR:ULTRASONIC:...` | DISTANCE/FAR/NO_RESPONSE/STUCK_HIGH/INVALID/ABSENT |
| `ERROR:ULTRASONIC_NO_RESPONSE` | 연속 비정상 응답 경고, 배선·환경·센서 원인 확인 |
| `ERROR:ULTRASONIC_STUCK_HIGH` | HIGH가 해제되지 않는 입력 경고 |
| `BUTTON_PRESSED` | 버튼, VoiceTrigger 및 기존 수신 처리 |
| `GREEN_LIGHT` | Pi의 GREEN 수신 |
| `CONNECTED` | notify 준비 후 상태 동기화 |
| `YOLO:ON/OFF` | Pi YOLO 상태 |

`포고핀 연결확인 중`은 전기 응답 확인 후 안정 확인/초기화 대기 상태입니다. 연결과 GET_STATUS에서 현재 포고핀 상태도 동기화합니다.

REG의 15초 마감은 카드 태그 직전에도 검사합니다. 만료 시 해당 태그로 등록하거나 활성 사용자를 변경하지 않습니다. 등록 카드가 새로 태그되면 일반 APPLY는 BLE 연결 없이도 가능합니다.

GET_STATUS는 현재 센서·계단 경고·모듈·ID 전압·활성 프로필·저장소·상부 장애물·YOLO·최대 가속도 간격을 보냅니다. 연결/조회 때 현재 계단 상태를 재전송하는 것은 동기화입니다.

## 점검 명령

`PING` → `PONG`. `GET_ACCEL`, `GET_DISTANCE`, `GET_STAIR_DISTANCE`, `GET_ULTRASONIC`은 거리/가속도와 미사용 시 -1을 반환합니다. `SET_STAIR_CM,30~600`은 임시 보정입니다. `LED_TEST`/`LED_ON`은 점검 순차 점등, `LED_OFF`는 자동 상태 표시를 유지합니다. `GET_SENSOR`는 미사용 ADC 때문에 `SENSOR:UNAVAILABLE`입니다.

## Pi UART

115200, 8N1, 3.3V 신호, GND 공통입니다. ESP32 TX17 → Pi UART RX, Pi UART TX → ESP32 RX16입니다.

- ESP32 → Pi: 상단 버튼마다 `BTN\n`
- Pi → ESP32: `GREEN\n`, `YOLO_ON\n`, `YOLO_OFF\n`

YOLO 모델·카메라·5분/1분 세션 관리는 기존 Pi 프로그램에서 수행합니다.

## API 확인 출처

- https://developer.android.com/reference/android/bluetooth/BluetoothGatt
- https://developer.android.com/reference/android/bluetooth/BluetoothGattCallback

Android 14에서는 첫 MTU 요청을 플랫폼이 517로 요청할 수 있으므로 요청값 자체보다 onMtuChanged의 실제 협상값을 사용합니다.
