# NAVCANE v4.2 — 한국어 진단·고장 경고 무진동, 클로드 검토용 통합 파일

이 파일 하나에 최신 ESP32 사용자 코드, Android 교체 파일 3개, 수정 드라이버, 문서와 테스트를 모았습니다. 실행·업로드는 NAVCANE_Integrated_v4.zip을 해제하고 platformio.ini가 있는 폴더를 PlatformIO에서 엽니다.

## 현재 동작과 수정 범위

v4.2는 고장 경고 진동을 끄고 로그·BLE 상태·자동 복구를 유지합니다. 진행 중인 낙상·초록불·카드 진동을 중단하거나 다시 시작하지 않습니다. 시리얼 이벤트·센서 원인·복구 단계·진동 이유를 한국어로 표시합니다. BLE 원문과 UART 규약·핀맵·감지 임계값은 유지합니다. 센서 응답 실패를 부품 고장으로 단정하지 않습니다. CORE_DEBUG_LEVEL=0으로 반복 Arduino 영문 로그를 줄이며 원문은 다시 켤 수 있습니다. 최초 부팅 시 미등록 인터럽트를 해제하던 호출도 막았습니다. 자세한 변경은 KOREAN_LOGS.md와 BUILD_LOG_SILENT_FAULTS.txt를 참고하세요.

v3 기반으로 후속 Claude diff의 장애물 재초기화·한글 계단 메시지·확장 고장 중복 진동 수정을 반영했습니다. diff에 없던 상수 정의와 현재 소스에 맞는 테스트도 포함했습니다.

- 마지막 낙상 한 개는 RAM에서 5분 미만만 보류합니다. 정확히 300,000ms부터 만료합니다. 단절/CCCD 비활성 상태에서도 만료를 검사하고 새 낙상은 새 시각으로 갱신합니다. 즉시 진동과 별도입니다. 발생 장소/보호자 수신을 보장하는 ACK는 없습니다.
- 장애물 거리값은 마지막 프레임 후 600ms부터 쓰지 않습니다. 측정 확인은 유지해 프레임이 돌아오면 재초기화 없이 재개합니다. 2.5초 이상 무프레임이면 다음 태스크 점검에서 즉시 초기화를 예약하고 실패 종료 후에만 5초 대기합니다. 초기화 자체의 시간이 사라지는 것은 아닙니다.
- L5CX 전용 Wire timeout=500ms, MPU·계단 공유 버스 timeout=25ms. LPn 200/500ms와 버스 재시작·클록/STOP 유지.
- 계단 감지는 계단 감지 한글 메시지, 해제는 STAIR_CLEARED 한 종류씩 사용합니다. 재연결/조회 시 현재 상태를 다시 보냅니다.
- 센서·확장모듈·슬롯 오류는 진동 없이 로그와 BLE 상태로 알립니다. feedbackStart(Fault)는 기존 진동 상태를 변경하지 않습니다.
- 3센서 태스크·독립 복구·전송 단위 I2C mutex·상부 거리 유지·낙상 진동 우선·CFG 대기/세대 검사·손상 파일 보존·앱 다중 수신자/줄 프레이밍을 유지합니다.

ESP32 새 폴더 빌드, C++ 회귀 테스트, Android API33 Kotlin 컴파일과 JVM 메시지 조립을 통과했습니다. v4.2 정적 RAM 64,628바이트, Flash 1,380,085바이트. 전체 APK 및 실물 검증은 하지 않았습니다. 15ms는 MPU 예약 목표이며 물리 버스/OS/플래시 조건까지 보장하는 고정 주기가 아닙니다.

## Claude에 요청할 검토

실제 경로/함수와 재현 조건을 들어 심각도순으로 검토해 주세요. 실제 확인된 결함과 실물 시험 가정을 구분해 주세요.

1. 600ms 데이터 신선도와 2500ms 드라이버 watchdog, 실패 종료 기준 backoff가 독립인지 확인해 주세요. ObstacleRecovery가 실제 태스크에서 사용됩니다.
2. 보류 낙상의 5분 마감·단절/CCCD OFF 만료·새 사건 갱신·millis wrap·프레임 경계와 낙상 우선순위를 확인해 주세요.
3. MPU/계단/L5CX 태스크, 공유 I2C 전송 단위 mutex, 고착 복구/센서 brownout을 확인해 주세요. 물리 공유 버스 고장이 완전히 격리된다고 가정하지 마세요.
4. 낙상 후보 600ms 마감, 샘플 신선도, 낙상 진동과 브레이크 우선순위를 확인해 주세요. 실제 낙상 정확도 검증으로 보지 마세요.
5. 상부 경고 거리 유지와 연속 감지/해제·중간 거리·무응답·600ms stale 처리, 슬롯 high-Z·100ms 분리·30ms 무리셋 재개를 확인해 주세요.
6. CFG 대기/등록 마감·BLE 세대 바뀜 직후 카드 쓰기, LittleFS 손상/기존 내용 보존과 검증/rename 실패를 확인해 주세요.
7. MTU 23/185, STREAM_ON UTF-8 프레이밍, 오래된 명령 폐기, 앱 수신자 충돌/GATT 실패/화면 종료를 확인해 주세요.
8. 실제 main.cpp 테스트 범위와 모듈별 테스트가 보장하지 않는 실행/하드웨어 조건을 구분해 주세요.

## 포함 범위

각 코드 블록은 해당 경로 파일 전체이며 SHA-256으로 ZIP과 대조했습니다. 사용자 src/include 전체, 앱 교체 3개, 수정 드라이버 5개, 문서/로그/테스트/스텁을 포함합니다. 외부 드라이버의 수정하지 않은 API와 센서 펌웨어 배열은 ZIP의 lib에 있습니다. 미제공 앱 Activity/XML/Manifest/실제 Firebase/보호자 전달 및 Pi 프로그램은 포함하지 않습니다.

과거 Claude v2 비교 기록은 CLAUDE_V2_COMPARISON.md로 구분합니다. NAVCANE_BEST 전체를 검토했다고 가정하지 마세요. 현재 결정은 CLAUDE_COMPARISON.md를 기준으로 합니다. 첨부 원문과 후속 diff는 ZIP reference에 보관했습니다.


## 파일: platformio.ini

SHA-256: `3444d2c72be1ef2584a54bff06f2604d49b1cf5522df6a14a99b1d48de2cfddc`

`````ini
; NAVCANE modular prototype — Arduino-ESP32 2.0.17
[platformio]
default_envs = esp32dev
[env:esp32dev]
platform = espressif32@6.9.0
board = esp32dev
framework = arduino
monitor_speed = 115200
monitor_filters = esp32_exception_decoder, time
board_build.partitions = min_spiffs.csv
board_build.filesystem = littlefs
build_flags =
  -DMFRC522_SPICLOCK=1000000u
  -DCORE_DEBUG_LEVEL=0
; Korean diagnostics replace noisy English Arduino errors. Use 1 for raw errors when debugging.
lib_deps =
  miguelbalboa/MFRC522@1.4.11
; Patched VL53L4CX and SparkFun VL53L5CX 1.0.3 are bundled in lib/.
`````

## 파일: README.md

SHA-256: `a6da8b603f07b48665f0b2e44645c7461745d3d2ad3895df92e41184c39a81e5`

`````markdown
# NAVCANE — 한국어 진단 보완 통합본 v4.2

첨부한 `BleManager.kt`, `RfidConfig.kt`, `RfidFragment.kt`의 장치명·UUID·명령·응답에 맞춘 ESP32 프로젝트입니다. 계단 센서는 **VL53L4CX**, 장애물 센서는 **VL53L5CX**, 가속도 센서는 **MPU6050**입니다. 계단 감지는 락 버튼 없이 항상 동작합니다.

ESP32 빌드와 PC 회귀 테스트를 통과했습니다. Android 교체 파일은 Android API 33 클래스와 Kotlin 컴파일러로 확인했고, 메시지 조립 로직의 JVM 테스트를 통과했습니다. 전체 Android 프로젝트·APK 빌드와 실물 센서/모터/휴대폰 시험은 수행하지 않았습니다.

## 고장 경고 진동 끄기

센서 통신 실패, 센서 미연결, 확장모듈 오류, 슬롯 ID 이상은 **진동 없이 한국어 로그와 BLE 상태로만 알립니다.** 이미 진행 중인 낙상·초록불·카드 진동도 고장 경고 때문에 중단하거나 다시 시작하지 않습니다. 낙상·계단·장애물·초록불·카드 처리 진동, 자동 복구, 핀맵과 앱 통신 규약은 유지합니다.

## 한국어 진단 보완

터미널 이벤트와 센서 상태·원인·자동 복구·진동 이유를 한국어로 표시합니다. BLE 규약과 핀맵은 유지합니다. 상세 설명과 표시 예시는 `docs/KOREAN_LOGS.md`를 참고하세요. 부팅 전 ROM/SDK/PlatformIO의 영문 출력은 일부 남을 수 있습니다.

## 사용 순서

1. ZIP 압축을 해제합니다.
2. VS Code → PlatformIO → **Open Project** → `NAVCANE_Integrated_v4` 폴더를 선택합니다. 바로 아래에 `platformio.ini`가 있습니다.
3. **Build → Upload → Monitor(115200)** 순서로 실행합니다. 포트는 자동 검색하며 필요하면 `upload_port`, `monitor_port`에 실제 COM 번호를 넣습니다.
4. Android 프로젝트의 `com.example.whitecaneguardian` 패키지에 `android_patch/`의 **세 파일을 함께 교체**합니다. 기존 화면 XML, Firebase 로그인·신체 정보 구조, `BleHolder`, `BleLog`, `VoiceTrigger`는 그대로 사용합니다. `compileSdk`는 33 이상이어야 합니다.
5. 앱 연결 → RFID 모듈 장착 → 설정 전송 → `ACK,CFG` 확인 → 카드 등록 → 카드 태그 순서로 사용합니다.

처음 빌드하면 ESP32 플랫폼과 RC522 라이브러리를 다운로드합니다. 두 ToF 드라이버는 프로젝트에 포함했습니다. 펌웨어 업로드에 `uploadfs`는 필요하지 않습니다.

## 이번 보완

- **보류 낙상은 발생 후 5분 미만만 재연결 시 전송합니다.** 정확히 5분(300,000ms)이 지나면 만료합니다. BLE 단절·알림 비활성 상태에서도 검사하고, 새 낙상이 발생하면 마지막 사건과 발생 시각으로 갱신합니다. 즉시 진동은 BLE 보관 만료와 별개입니다. 만료된 사건으로 보호자 알림을 발생시키지 않는 정책입니다.
- **장애물 거리값은 600ms부터 사용하지 않습니다.** 센서 태스크는 측정을 계속 확인하므로 프레임이 일찍 돌아오면 펌웨어 재업로드 없이 감지를 재개합니다. **2.5초 이상 무프레임이면 태스크 점검 시 바로 재초기화를 예약**합니다. v3의 추가 5초 대기를 제거했습니다. 5초 대기는 초기화 실패 종료 후 재시도에만 사용합니다. 초기화 자체에는 시간이 걸립니다.
- L5CX 전용 Wire의 I2C timeout은 기존 정상 동작 코드의 **500ms**, MPU·계단 공유 버스는 **25ms**로 분리했습니다. LPn LOW 200ms/HIGH 500ms 준비와 버스 재시작·클록/STOP 복구를 유지합니다. 실제 부품/배선의 안정성은 실물 시험 대상입니다.
- 계단 감지는 **`계단 감지`**, 해제는 `STAIR_CLEARED`만 보냅니다. `STAIR_DETECTED`를 중복해서 보내지 않습니다. 현재 상태 조회/재연결 때는 같은 메시지로 상태를 동기화합니다.
- 센서·확장모듈·슬롯 ID의 고장 경고 진동을 공통 진동 처리 함수에서 차단합니다. 각 모듈의 로그·상태 전송은 유지합니다.
- 상부 경고 거리 유지, 600ms 신선도, 포고핀 안정 확인, 슬롯 흔들림 후 무리셋 재개, CFG 대기, 카드 태그 직전 등록 마감/세대 검증, CRC 저장소 보호를 유지합니다.

5분은 오래된 사건 재전송을 줄이기 위한 정책값이며 발생 당시 위치를 저장하는 기능은 아닙니다. 수신/보호자 전달 ACK도 아닙니다. 전체 앱의 위치·보호자 전달 구현은 제공되지 않았습니다. 비교와 결정은 `docs/CLAUDE_COMPARISON.md`에 정리했습니다.

## 유지한 동작

- **CFG는 대기 설정**입니다. 전송만으로 현재 사용자의 거리·진동 설정을 바꾸지 않습니다. 카드 등록 성공 또는 등록 카드 인식 시 적용합니다.
- MPU, 계단 ToF, 장애물 ToF는 **세 태스크**에서 처리합니다. MPU는 15ms를 목표로 주기를 예약하며, 계단 센서 초기화 전체를 기다리지 않습니다. 공유 I2C는 전송 한 건씩 잠급니다.
- MPU와 계단 ToF의 정상 여부·복구를 독립적으로 관리합니다. 계단 ToF가 없거나 모델이 다르더라도 정상 MPU를 함께 끄지 않습니다.
- 포고핀 센서는 연결이 3초 유지된 뒤, 200ms 간격의 정상 확인 5회를 거쳐 초기화합니다. 각 센서가 독립적으로 이 절차를 거칩니다.
- 슬롯 ID 이상을 관측하면 공유 핀 출력을 즉시 중지합니다. **100ms 이상 다른 ID가 지속될 때만 논리적 분리**로 처리합니다. 짧은 흔들림 후 원래 ID가 30ms 안정되면 RC522 소프트 리셋·분리 로그 없이 재개하고 상태를 점검합니다. 새 모듈은 300ms 안정 후 활성화합니다.
- 초음파는 60ms 동안 응답을 관측합니다. 정상적으로 끝난 긴 ECHO는 거리 범위 밖으로 처리하고 고장 횟수에 넣지 않습니다. 응답 없음, HIGH 고정, 비정상 펄스는 별도로 분류합니다.
- 상부 장애물은 **120cm 미만 2회로 경고**, **130cm 초과/범위 밖 2회로 해제**합니다. 좌우 모터를 함께 구동합니다. 이 값은 하부 장애물의 개인 설정 20~200cm와 별개입니다.
- 계단 감지와 해제는 각각 `계단 감지`, `STAIR_CLEARED` 한 종류만 전송합니다. `포고핀 분리상태`, `포고핀 재결합`을 복원했습니다.
- LED는 켜짐=정상/활성으로 통일했습니다. **Q0 장애물 ToF, Q1 MPU, Q2 계단 ToF, Q3 BLE, Q4 YOLO**입니다.
- 낙상 진동은 계단 경고가 들어와도 취소되지 않습니다. 브레이크 제어는 진동 패턴과 별개로 계단 상태를 따릅니다.
- 앱 수신자는 여러 개를 등록할 수 있습니다. RFID 화면이 주 수신자를 덮어쓰지 않습니다. 보호자 알림을 처리하던 주 수신자는 계속 메시지를 받습니다.
- 기존 앱에는 기본적으로 한 이벤트를 한 notify로 전송합니다. 교체 앱은 `STREAM_ON`으로 줄바꿈 프레이밍을 요청하므로 MTU 23에서도 긴 한글·RFID 메시지를 안전하게 조립합니다.

## 핀 연결

| GPIO | 용도 |
|---|---|
| 18 / 19 | VL53L5CX SDA / SCL, Wire |
| 25 | VL53L5CX LPn |
| 21 / 22 | MPU6050 + VL53L4CX SDA / SCL, I2C 1 |
| 32 / 14 / 26 | 중앙 / 좌 / 우 진동 드라이버 입력 |
| 23 | 브레이크 서보 신호 |
| 16 / 17 | Pi UART RX / TX, 115200 |
| 33 | 상단 버튼, 눌렀을 때 GND |
| 36 | 슬롯 ID ADC, 본체 10kΩ → 3.3V |
| 4 / 13 / 35 / 5 | 슬롯 S1 / S2 / S3 / S4 |

GPIO2·27·34 출력은 사용하지 않습니다. BLE·YOLO LED는 74HC595 모듈에 표시합니다.

| 슬롯 | RFID RC522 | 리모컨+HC-SR04P | LED 74HC595 |
|---|---|---|---|
| ID 저항→GND | 2.2kΩ | 10kΩ | 33kΩ |
| S1 / GPIO4 | SCK | TRIG | CLK |
| S2 / GPIO13 | MOSI | PC817 입력 | DATA |
| S3 / GPIO35 | MISO | ECHO | 미사용 |
| S4 / GPIO5 | SS(SDA) | 미사용 | LATCH |
| 내부 연결 | RST→모듈 3.3V, IRQ 미사용 | 3.3V 대응 ECHO | /MR=HIGH, /OE=LOW |

슬롯은 3.3V/GND/ID/S1/S2/S3/S4의 7핀입니다. S1·S2·S4 직렬 330Ω, 입력 전용 S3의 10kΩ 풀다운 구성을 유지합니다. 각 LED와 PC817 입력에는 전류 제한 저항을 사용합니다. 슬롯 전원은 상시 공급이며 high-Z 처리는 신호 출력 보호입니다.

## 처음 실행할 때 조정할 값

`include/config.h`에 모아 두었습니다.

- 브레이크 각도는 기존 `SERVO_RELEASE_DEG=0`, `SERVO_APPLY_DEG=20`을 유지했습니다. 실제 기구의 방향과 끝 위치를 확인하세요. 서보 전원은 해당 모델의 정격 전원이며 ESP32와 GND를 공통으로 연결합니다.
- 기본 계단 임계값은 센서에서 바닥까지의 사선 거리 **170cm**입니다. 키 자체가 아닙니다. 키 코드별 실측 자료가 없으므로 14~19의 표를 모두 170으로 두었습니다.
- `SET_STAIR_CM,값`은 RAM 임시 보정입니다. 카드에 저장되지 않으며 카드 적용·재부팅 시 초기화됩니다. 카드별 기본 계단 기준은 `STAIR_CM_BY_HEIGHT`의 실측표를 사용합니다.
- 장애물 관심 행은 0~2이며 `SWAP_OBSTACLE_SIDES`로 실제 장착 방향을 맞춥니다. 좌우 거리와 계단 유효 거리에 3회 이동 평균을 사용합니다.
- VL53L4CX는 4선 브레이크아웃, XSHUT가 HIGH로 유지되는 배선입니다. 모델 ID **0xEBAA**를 확인합니다. VL53L0X/L1X는 이 펌웨어의 계단 센서가 아닙니다.
- HC-SR04P의 범위 밖 ECHO는 실제 부품에서 확인하세요. 60ms 안에 끝난 정상 긴 펄스와 60ms까지 내려오지 않는 입력을 구분합니다.

## 저장소 오류 복구

저장 파일은 CRC와 임시 파일 검증 후 rename으로 교체합니다. 기존 28바이트 레코드 파일도 읽습니다. 손상된 파일이나 기존 데이터가 있는 마운트 실패 파티션은 자동으로 지우지 않습니다.

처음 설치하면서 파티션 위치가 바뀌면 이전 앱 데이터 때문에 `[PROFILE] mount failed` 또는 `REG,FSERR`가 나올 수 있습니다. 저장된 카드가 필요하면 먼저 백업하세요. **저장소를 비워도 되는 경우에만** 시리얼 모니터에서 다음 한 줄을 줄바꿈과 함께 직접 입력합니다.

```text
FS_INIT_CONFIRM_ERASE_PROFILES
```

이 명령은 오류 상태의 프로필 파일시스템을 초기화합니다. 정상 저장소에서는 거부하며 BLE로는 실행되지 않습니다. 초기화가 성공하면 재부팅이나 전체 플래시 삭제 없이 등록할 수 있습니다. 빈 파티션은 최초 부팅 때 자동 포맷합니다.

활성 사용자 설정은 RAM이므로 재부팅 시 기본값입니다. 카드로 다시 적용합니다. CRC 형식으로 저장한 후 옛 펌웨어로 돌아갈 경우 파일 형식이 호환되지 않을 수 있으므로 먼저 백업하세요.

## 파일 안내

- `src/`, `include/`, `lib/`, `platformio.ini`: ESP32 프로젝트
- `android_patch/`: 기존 앱에 교체할 Kotlin 파일 3개
- `NAVCANE_v4_Claude_Review.md`: 클로드에 첨부할 단일 검토 파일
- `docs/CLAUDE_COMPARISON.md`: 두 버전 비교와 채택 근거
- `docs/APP_INTEGRATION.md`: BLE·RFID·Pi 규약
- `docs/VALIDATION.md`: 실제 검증 결과와 실물 확인 순서
- `docs/CHANGES.md`: 수정 결정과 드라이버 출처
- `test/run_host_tests.py`: 실제 C++ 구현의 PC 회귀 테스트
- `test/test_main.cpp`: 실제 메인 코드의 경고 중복·거리 신선도·한글 계단 이벤트 검사
- `test/NavcaneProtocolTest.kt`: 실제 앱 메시지 조립·RFID 상수 JVM 테스트

PC 테스트는 Python 3와 g++가 있는 환경에서 `python test/run_host_tests.py`로 실행합니다.
`````

## 파일: include/ble_link.h

SHA-256: `a70f7fcda2a8c010d77ab28f73054bf9e448681e09d148e2ccb39312e409eb03`

`````cpp
#pragma once
#include <Arduino.h>
void bleBegin();
void bleTick();
bool blePopCommand(String &command);
void sendMessage(const String &message);
void sendProtocolMessage(const String &message);
bool bleConnected();
uint32_t bleSessionEpoch();
bool bleTakeSync();

void bleEnableStream();
`````

## 파일: include/config.h

SHA-256: `5fb6b897d3a4e69c3d653e382be0d7fff2b1208e903a8c7ef0f232d54fe13e32`

`````cpp
#pragma once
#include <stdint.h>
namespace cfg {
constexpr bool SERIAL_RAW_PROTOCOL = false; // true also prints the original BLE event on Serial
constexpr char DEVICE_NAME[] = "NAVCANE_ESP32";
constexpr char SERVICE_UUID[] = "12345678-1234-1234-1234-1234567890ab";
constexpr char CHARACTERISTIC_UUID[] = "abcd1234-1234-1234-1234-abcdef123456";
constexpr uint8_t OBSTACLE_SDA = 18, OBSTACLE_SCL = 19, OBSTACLE_LPN = 25;
constexpr uint8_t SEGMENT_SDA = 21, SEGMENT_SCL = 22;
constexpr uint8_t VIB_MAIN = 32, VIB_LEFT = 14, VIB_RIGHT = 26;
constexpr uint8_t VIB_CH_MAIN = 0, VIB_CH_LEFT = 2, VIB_CH_RIGHT = 4;
constexpr uint8_t SERVO = 23, SERVO_CHANNEL = 7, BUTTON = 33;
constexpr uint8_t UART_RX = 16, UART_TX = 17;
constexpr uint8_t SLOT_ID = 36, S1 = 4, S2 = 13, S3 = 35, S4 = 5;
constexpr uint32_t ID_SETTLE_MS = 300, ID_POLL_MS = 5;
constexpr uint32_t ID_LEAVE_MS = 100, ID_RESUME_MS = 30;
constexpr uint32_t POGO_SETTLE_MS = 3000, POGO_PROBE_MS = 200;
constexpr uint8_t POGO_STABLE_COUNT = 5;
constexpr uint32_t MPU_PERIOD_MS = 15;
// Expected divider voltages: 595 / 1650 / 2533 mV. Calibrate if needed.
constexpr uint16_t RFID_ID_MIN_MV = 380, RFID_ID_MAX_MV = 850;
constexpr uint16_t REMOTE_ID_MIN_MV = 1250, REMOTE_ID_MAX_MV = 2050;
constexpr uint16_t LED_ID_MIN_MV = 2250, LED_ID_MAX_MV = 2850;
constexpr uint32_t I2C_TIMEOUT_MS = 25, SENSOR_RETRY_MS = 5000;
// Separate buses: preserve the original L5CX timeout without delaying shared MPU I2C.
constexpr uint32_t OBSTACLE_I2C_TIMEOUT_MS = 500;
constexpr uint32_t STAIR_STALE_MS = 600, OBSTACLE_STALE_MS = 600, ACCEL_STALE_MS = 120;
constexpr uint32_t OBSTACLE_REINIT_MS = 2500;
// At age >= 5 min the pending BLE event expires, even while disconnected.
constexpr uint32_t FALL_PENDING_MAX_MS = 5u * 60u * 1000u;
constexpr uint32_t SENSOR_HEALTH_MS = 500;
constexpr uint8_t SENSOR_HEALTH_MISS_LIMIT = 2;
constexpr uint32_t OBSTACLE_LPN_LOW_MS = 200, OBSTACLE_BOOT_MS = 500;
constexpr uint8_t OBSTACLE_ROW_MIN = 0, OBSTACLE_ROW_MAX = 2;
constexpr bool SWAP_OBSTACLE_SIDES = false;
constexpr uint16_t VIB_WEAK_DUTY = 150, VIB_STRONG_DUTY = 255;
constexpr int SERVO_RELEASE_DEG = 0, SERVO_APPLY_DEG = 20;
constexpr uint16_t SERVO_MIN_US = 500, SERVO_MAX_US = 2400;
constexpr uint16_t DEFAULT_OBSTACLE_CM = 50;
constexpr float DEFAULT_STAIR_CM = 170.0f;
// Calibrate on the assembled cane. Height alone is NOT the floor slant distance.
constexpr float STAIR_CM_BY_HEIGHT[6] = {170, 170, 170, 170, 170, 170};
constexpr uint8_t DEFAULT_HEIGHT_CODE = 17, DEFAULT_VIB_LEVEL = 1;
constexpr uint32_t REG_WAIT_MS = 15000;
constexpr bool STAIR_NO_TARGET_ALERT = true;
constexpr uint32_t REMOTE_PULSE_MS = 200;
constexpr uint32_t ULTRASONIC_PERIOD_MS = 100, ULTRASONIC_TIMEOUT_US = 60000;
constexpr uint8_t ULTRASONIC_MISS_LIMIT = 10;
constexpr float ULTRASONIC_MIN_CM = 2, ULTRASONIC_MAX_CM = 400;
constexpr float UPPER_OBSTACLE_CM = 120, UPPER_RELEASE_CM = 130;
}
`````

## 파일: include/expansion.h

SHA-256: `085e04a7e515cb0331fa5ff839d35f79409106ac0c9fa24c44347511069bc0cc`

`````cpp
#pragma once
#include <Arduino.h>
#include "logic.h"
void expansionBegin();
void expansionTick(uint8_t ledHealthMask);
void expansionRemotePress();
void expansionLedTest();
bool expansionRfidReady();
nav::Module expansionModule();
uint16_t expansionIdMv();
float expansionUltrasonicCm();
bool expansionFault();

bool expansionUpperAlert();
float expansionUpperDistance();
const char *expansionEchoState();
`````

## 파일: include/feedback.h

SHA-256: `f116c0c1f32d346d8dac3232096d1d8032b45ff04a2f0f0d5cae3a4792c8768c`

`````cpp
#pragma once
#include <Arduino.h>
enum class Feedback : uint8_t { Green, Applied, Unregistered, Registered, Failed, Fault, Fall };
void feedbackBegin();
void feedbackStart(Feedback pattern);
void feedbackTick(bool stairAlert, float leftCm, float rightCm, float frontCm);
void brakeSet(bool active);
`````

## 파일: include/logic.h

SHA-256: `c920644fa056fee1d9cc6905e807a0551ef4aa8289820b3a94354e4fedb51c06`

`````cpp
#pragma once
#include <stdint.h>
#include <math.h>
#include "config.h"
namespace nav {
enum class Module : uint8_t { None, Rfid, Remote, Led, Unknown };
inline Module identify(uint16_t mv) {
  if (mv >= cfg::RFID_ID_MIN_MV && mv <= cfg::RFID_ID_MAX_MV) return Module::Rfid;
  if (mv >= cfg::REMOTE_ID_MIN_MV && mv <= cfg::REMOTE_ID_MAX_MV) return Module::Remote;
  if (mv >= cfg::LED_ID_MIN_MV && mv <= cfg::LED_ID_MAX_MV) return Module::Led;
  return mv >= 2950 ? Module::None : Module::Unknown;
}
inline bool elapsed(uint32_t now, uint32_t then, uint32_t period) {
  return uint32_t(now - then) >= period;
}
// Keeping the driver alive lets a transient missing frame recover without firmware upload.
// Only a failed initialization uses backoff; a stalled running sensor restarts immediately.
class ObstacleRecovery {
 public:
  bool running() const { return ready; }
  bool shouldInitialize(uint32_t now) const {
    return !ready && (!attempted || elapsed(now, attemptAt, cfg::SENSOR_RETRY_MS));
  }
  void initialized(bool ok, uint32_t completedAt) {
    ready = ok; attempted = true; attemptAt = frameAt = completedAt;
  }
  void frame(uint32_t now) { frameAt = now; }
  bool needsRestart(uint32_t now) {
    if (!ready || !elapsed(now, frameAt, cfg::OBSTACLE_REINIT_MS)) return false;
    ready = false; attempted = false; return true;
  }
 private:
  bool ready = false, attempted = false;
  uint32_t attemptAt = 0, frameAt = 0;
};
// Electrical permission and logical attachment are intentionally separate.
// First mismatch revokes outputs; only a sustained mismatch detaches the module.
class SlotGate {
 public:
  Module active = Module::None;
  bool permitted = false;
  bool update(Module observed, uint32_t now) {
    const Module old = active;
    if (observed != candidate) { candidate = observed; since = now; }
    if (active != Module::None) {
      if (observed != active) {
        permitted = false; returning = false;
        if (!leaving) { leaving = true; leaveAt = now; }
        if (elapsed(now, leaveAt, cfg::ID_LEAVE_MS)) { active = Module::None; leaving = false; }
      } else {
        leaving = false;
        if (!permitted) {
          if (!returning) { returning = true; returnAt = now; }
          if (elapsed(now, returnAt, cfg::ID_RESUME_MS)) { permitted = true; returning = false; }
        }
      }
    }
    if (active == Module::None && supported(candidate) && elapsed(now, since, cfg::ID_SETTLE_MS)) {
      active = candidate; permitted = true; leaving = returning = false;
    }
    return old != active;
  }
 private:
  static bool supported(Module m) { return m == Module::Rfid || m == Module::Remote || m == Module::Led; }
  Module candidate = Module::None;
  uint32_t since = 0, leaveAt = 0, returnAt = 0;
  bool leaving = false, returning = false;
};
// Wait for continuous electrical presence, then five independently spaced probes.
class ReconnectGate {
 public:
  bool update(bool present, uint32_t now) {
    if (!present) { reset(); return false; }
    if (!seen) { seen = true; since = now; last = now; }
    if (!elapsed(now, since, cfg::POGO_SETTLE_MS)) return false;
    if (count < cfg::POGO_STABLE_COUNT && elapsed(now, last, cfg::POGO_PROBE_MS)) {
      last = now; ++count;
    }
    return count >= cfg::POGO_STABLE_COUNT;
  }
  void reset() { seen = false; count = 0; }
 private:
  bool seen = false;
  uint8_t count = 0;
  uint32_t since = 0, last = 0;
};
enum class EchoResult { Distance, Far, NoResponse, StuckHigh, Invalid };
inline EchoResult classifyEcho(bool done, bool sawRise, bool high, uint32_t width) {
  if (!done) return high || sawRise ? EchoResult::StuckHigh : EchoResult::NoResponse;
  if (width < 60 || width > cfg::ULTRASONIC_TIMEOUT_US) return EchoResult::Invalid;
  float cm = width * 0.0343f / 2;
  return cm < cfg::ULTRASONIC_MIN_CM || cm > cfg::ULTRASONIC_MAX_CM ? EchoResult::Far : EchoResult::Distance;
}
class UpperDecision {
 public:
  bool alert = false;
  float warningCm = -1;
  void reset() { alert = false; warningCm = -1; above = below = 0; }
  // Missing samples break confirmation but cannot prove the obstacle disappeared.
  void invalid() { above = below = 0; }
  void sample(float cm) {
    if (!isfinite(cm)) { invalid(); return; }
    if (cm >= 0 && cm < cfg::UPPER_OBSTACLE_CM) {
      below = 0; if (above < 2) ++above; if (above >= 2) alert = true;
      if (alert) warningCm = cm;
    } else if (cm < 0 || cm > cfg::UPPER_RELEASE_CM) {
      above = 0; if (below < 2) ++below;
      if (below >= 2) { alert = false; warningCm = -1; }
    } else {
      above = below = 0;
      if (alert) warningCm = cm;
    }
  }
 private:
  uint8_t above = 0, below = 0;
};
inline uint8_t normalLeds(bool obstacle, bool mpu, bool stair, bool ble, bool yolo) {
  return (obstacle?1:0) | (mpu?2:0) | (stair?4:0) | (ble?8:0) | (yolo?16:0);
}
class Mean3 {
 public:
  void reset() { count = next = 0; }
  float sample(float value) {
    if (!isfinite(value) || value < 0) { reset(); return -1; }
    values[next] = value; next = (next + 1) % 3;
    if (count < 3) ++count;
    float sum = 0; for (uint8_t i=0; i<count; ++i) sum += values[i];
    return sum/count;
  }
 private:
  float values[3] = {};
  uint8_t count = 0, next = 0;
};
class StairDecision {
 public:
  bool alert = false;
  void reset() { alert = false; above = below = invalid = 0; }
  void sample(float cm, bool noTarget, float threshold) {
    const bool risk = (isfinite(cm) && cm >= 0 && cm > threshold)
                      || (noTarget && cfg::STAIR_NO_TARGET_ALERT);
    if (risk) {
      below = 0; invalid = 0;
      if (above < 2) ++above;
      if (above >= 2) alert = true;
    } else if (isfinite(cm) && cm >= 0) {
      above = 0; invalid = 0;
      if (cm < threshold - 5) {
        if (below < 3) ++below;
        if (below >= 3) alert = false;
      } else below = 0;
    } else {
      above = below = 0;
      if (invalid < 3) ++invalid;
      if (invalid >= 3) alert = true;
    }
  }
 private:
  uint8_t above = 0, below = 0, invalid = 0;
};
inline float accelMagnitude(const uint8_t *raw) {
  float sum = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    int16_t value = int16_t((uint16_t(raw[2*i]) << 8) | raw[2*i+1]);
    float acceleration = value*(9.80665f/4096.0f); // +/-8g
    sum += acceleration*acceleration;
  }
  return sqrtf(sum);
}
class FallDetector {
 public:
  void reset() { state = 0; }
  bool sample(float magnitude, uint32_t now) {
    if (!isfinite(magnitude)) { reset(); return false; }
    switch (state) {
      case 0: if (magnitude < 5.0f) { state = 1; since = now; } break;
      case 1:
        // Check the deadline before accepting an impact from a later movement.
        if (elapsed(now, since, 600)) reset();
        else if (magnitude > 10.0f) { state = 2; since = now; }
        break;
      case 2:
        if (!elapsed(now, since, 500)) break;
        if (magnitude < 7.0f || magnitude > 12.5f) reset();
        else if (elapsed(now, since, 1500)) { reset(); return true; }
        break;
    }
    return false;
  }
 private:
  uint8_t state = 0;
  uint32_t since = 0;
};
}
`````

## 파일: include/profiles.h

SHA-256: `6995d327d1db4786922bfa8feaaf1b349e9f30779f23b986df9d454de4a83293`

`````cpp
#pragma once
#include <Arduino.h>
extern float activeObstacleThresholdCM;
extern float activeStairThresholdCM;
extern uint32_t activeVibDuty;
extern char activeProfileName[13];
void profilesBegin();
bool profilesCommand(const String &command);
void profilesCard(const uint8_t *uid, uint8_t length);
void profilesTick();
void profilesCancelRegistration(const char *reason);
bool profilesWaiting();

bool profilesStoreReady();
void profilesClearPending();
void profilesSerialCommand(const String &command);
`````

## 파일: include/segment_bus.h

SHA-256: `e3c607ff55c945422aec2c47128400fb683d96c202e9fb21a68cfab97b4a6df4`

`````cpp
#pragma once
// Hooks called by the vendored ST transport once per complete I2C transaction.
extern "C" bool navSegmentLock();
extern "C" void navSegmentUnlock();
`````

## 파일: include/sensors.h

SHA-256: `6f5751d4d87d88c8db6f038c07c649f4a26b4dd800d472f5cb5b9909b523dd22`

`````cpp
#pragma once
#include <Arduino.h>
struct SensorSnapshot {
  bool mpuConnected = false, stairConnected = false;
  bool mpuReady = false, stairReady = false, obstacleReady = false;
  float accel = 0, stairCm = -1, leftCm = -1, rightCm = -1;
  bool stairNoTarget = false;
  const char *mpuReason = "기동 중: 연결 확인 대기";
  const char *stairReason = "기동 중: 연결 확인 대기";
  const char *obstacleReason = "기동 중: 초기화 대기";
  uint32_t accelAt = 0, stairAt = 0, obstacleAt = 0;
  uint32_t accelSeq = 0, stairSeq = 0, obstacleSeq = 0;
  uint32_t fallCount = 0, maxAccelGapMs = 0;
};
bool sensorsBegin();
SensorSnapshot sensorSnapshot();
`````

## 파일: include/serial_text.h

SHA-256: `35124d46fd38283a0b9476178eb226c91120cfa3cf98b5d56352b60736f6185f`

`````cpp
#pragma once
#include <Arduino.h>

// Serial descriptions only. Never use these strings as BLE protocol messages.
namespace nav {
inline String eventField(const String &line, unsigned wanted) {
  int start = 0;
  for (unsigned i = 0; i < wanted; ++i) {
    int comma = line.indexOf(',', start);
    if (comma < 0) return "";
    start = comma + 1;
  }
  int end = line.indexOf(',', start);
  return end < 0 ? line.substring(start) : line.substring(start, end);
}
inline String moduleKorean(const String &name) {
  if (name == "RFID") return "RFID 카드 모듈";
  if (name == "REMOTE") return "리모컨·초음파 모듈";
  if (name == "LED") return "LED 표시 모듈";
  if (name == "NONE") return "모듈 없음";
  return "종류를 판별할 수 없는 모듈";
}
inline String cancelKorean(const String &reason) {
  if (reason == "USER") return "사용자 요청";
  if (reason == "DETACHED") return "확장모듈 연결 끊김";
  if (reason == "RFID_FAULT") return "RFID 통신 이상";
  if (reason == "DISCONNECTED") return "휴대폰 연결 끊김";
  if (reason == "CONFIG_CHANGED") return "대기 설정 변경";
  return "등록 조건 변경";
}
inline String serialEventText(const String &s) {
  if (s == "FALL_DETECTED") return "[낙상] 낙상으로 판정했습니다. 중앙 진동과 휴대폰 알림을 요청합니다.";
  if (s == "계단 감지" || s == "STAIR_DETECTED") return "[계단] 위험 감지 상태입니다. 중앙 진동과 브레이크를 작동합니다.";
  if (s == "STAIR_CLEARED") return "[계단] 위험 해제 상태입니다.";
  if (s == "포고핀 재결합") return "[포고핀] 분절 센서 통신이 복구됐습니다. 실제 접촉을 직접 측정한 결과는 아닙니다.";
  if (s == "포고핀 분리상태") return "[포고핀] 분절 센서 응답이 끊겼습니다. 접점·전원·통신을 확인하세요.";
  if (s == "포고핀 연결확인 중") return "[포고핀] 센서 응답을 확인했습니다. 연결 안정 확인·초기화 중입니다.";
  if (s == "OBSTACLE:LEFT") return "[장애물] 왼쪽 장애물 감지";
  if (s == "OBSTACLE:RIGHT") return "[장애물] 오른쪽 장애물 감지";
  if (s == "OBSTACLE:BOTH") return "[장애물] 양쪽 장애물 감지";
  if (s == "OBSTACLE:NONE") return "[장애물] 좌우 장애물 경고 해제 상태";
  if (s == "OBSTACLE:UPPER") return "[장애물] 상부 장애물 감지";
  if (s == "OBSTACLE:UPPER_CLEAR") return "[장애물] 상부 장애물 경고 해제 상태";
  if (s == "BUTTON_PRESSED") return "[버튼] 상단 버튼 입력. Pi와 휴대폰에 전달합니다. GPIO33은 버튼, 중앙 진동은 GPIO32입니다.";
  if (s == "GREEN_LIGHT") return "[신호등] Pi에서 초록불 감지 신호를 받았습니다.";
  if (s == "YOLO:ON") return "[영상인식] YOLO 활성 상태";
  if (s == "YOLO:OFF") return "[영상인식] YOLO 비활성 상태";
  if (s == "CONNECTED") return "[휴대폰] BLE 알림 준비 완료. 현재 상태를 동기화합니다.";
  if (s == "PONG") return "[통신] 통신 확인 요청에 응답했습니다.";
  if (s == "ACK,STREAM") return "[통신] 줄바꿈 메시지 조립 모드를 사용합니다.";
  if (s == "ACK,CFG") return "[설정] 카드 등록용 대기 설정 저장 완료. 현재 사용자 설정은 아직 바뀌지 않았습니다.";
  if (s.startsWith("ACK,STAIR,")) return "[보정] 임시 계단 기준 거리: " + eventField(s,2) + "cm";
  if (s.startsWith("REG,WAIT,")) return "[카드] 등록 대기 중. " + eventField(s,2) + "초 안에 카드를 태그하세요.";
  if (s.startsWith("REG,OK,")) return "[카드] 등록 성공: " + eventField(s,4) + (eventField(s,3)=="UPDATE" ? " (기존 카드 갱신)" : " (신규 카드)");
  if (s == "REG,NOCFG") return "[카드 오류] 대기 설정이 없습니다. 앱에서 설정을 먼저 전송하세요.";
  if (s == "REG,NO_RFID") return "[카드 오류] RFID 모듈이 준비되지 않았습니다. 슬롯 배선과 모듈을 확인하세요.";
  if (s == "REG,FSERR") return "[카드 오류] 저장소 이상으로 등록할 수 없습니다. 기존 데이터를 자동 삭제하지 않습니다.";
  if (s == "REG,FULL") return "[카드 오류] 등록 한도 500명에 도달했습니다.";
  if (s == "REG,TIMEOUT") return "[카드] 등록 대기 시간이 끝났습니다. 다시 등록을 시작하세요.";
  if (s.startsWith("REG,CANCEL,")) return "[카드] 등록 취소: " + cancelKorean(eventField(s,2));
  if (s.startsWith("APPLY,")) return "[카드] 저장된 사용자 설정 적용: " + eventField(s,2);
  if (s.startsWith("UNREG,")) return "[카드] 미등록 카드입니다. 기본 사용자 설정을 적용합니다.";
  if (s.startsWith("ERR,CFG,")) return "[설정 오류] 형식 또는 범위를 확인하세요. 이름 영문·숫자 1~12자, 거리 20~200cm, 키 코드 14~19, 진동 0/1.";
  if (s == "ERR,STAIR,RANGE") return "[보정 오류] 계단 기준 거리는 30~600cm입니다.";
  if (s.startsWith("ERR,QUEUE,")) return "[통신 오류] 명령 대기열이 가득 찼습니다. 잠시 후 다시 요청하세요.";
  if (s.startsWith("ERR,COMMAND,")) return "[통신 오류] 명령이 너무 길거나 형식이 잘못됐습니다. 최대 96바이트입니다.";
  if (s == "ERROR:MTU_LOW") return "[통신 경고] 메시지 전송 크기가 작습니다. 앱의 MTU 협상 또는 줄바꿈 조립 모드를 확인하세요.";
  if (s == "ERROR:UNKNOWN_COMMAND") return "[명령 오류] 지원하지 않는 명령입니다.";
  if (s == "ERROR:ULTRASONIC_STUCK_HIGH") return "[초음파 오류] ECHO가 HIGH에 머물렀습니다. ECHO 배선·전압·센서 또는 슬롯 오인식을 확인하세요.";
  if (s == "ERROR:ULTRASONIC_NO_RESPONSE") return "[초음파 오류] 비정상 응답이 반복됩니다. TRIG·ECHO·전원·슬롯 ID를 확인하세요.";
  if (s == "MODULE,UNKNOWN,ID_INVALID") return "[슬롯 오류] ID 전압으로 모듈 종류를 판별할 수 없습니다. 슬롯 미구성이라면 GPIO36의 외부 10kΩ 풀업이 필요합니다. 고장 경고 진동은 사용하지 않습니다.";
  if (s.startsWith("MODULE,")) {
    String state = eventField(s,2);
    String description = state=="READY" ? "사용 준비 완료" : state=="INITIALIZING" ? "초기화 중" :
                         state=="FAULT" ? "통신 이상. 분리·재장착 또는 배선을 확인하세요." : "연결 상태";
    return "[슬롯] " + moduleKorean(eventField(s,1)) + ": " + description;
  }
  if (s == "REMOTE,TRIGGERED") return "[리모컨] 버튼 동작 신호를 출력했습니다.";
  if (s == "LED,TEST") return "[LED] 표시등 순차 점등 시험 시작";
  if (s == "ERR,LED,NO_MODULE") return "[LED 오류] LED 슬롯 모듈이 연결되지 않았습니다.";
  if (s == "LED,AUTO_HEALTH") return "[LED] 센서 정상·BLE·YOLO 상태를 자동 표시합니다.";
  if (s == "LED:MODULE") return "[LED] LED 모듈 연결 상태";
  if (s == "LED:ABSENT") return "[LED] LED 모듈 없음";
  if (s.startsWith("SENSOR:")) {
    int end = s.indexOf(':',7);
    if (end >= 0) {
      String name=s.substring(7,end), state=s.substring(end+1);
      String label=name=="MPU" ? "가속도 MPU6050" : name=="STAIR" ? "계단 VL53L4CX" :
                   name=="OBSTACLE" ? "장애물 VL53L5CX" : name=="PROFILE_STORE" ? "카드 저장소" :
                   name=="ULTRASONIC" ? "상부 초음파" : name=="STAIR_ALERT" ? "계단 경고" : name;
      String detail=state=="READY" ? "사용 가능" : state=="FAULT" ? "사용 불가. 미연결·통신 실패·측정 갱신 지연 여부는 상태/원인 로그를 확인하세요." :
                    state=="ABSENT" ? "모듈 없음" : state=="FAR" ? "측정 범위 밖" :
                    state=="DISTANCE" ? "거리 측정 중" : state=="NO_RESPONSE" ? "응답 없음" :
                    state=="STUCK_HIGH" ? "ECHO가 HIGH에 고정됨" : state=="INVALID" ? "비정상 펄스" :
                    state=="1" ? "위험 감지 상태" : state=="0" ? "위험 해제 상태" : state;
      return "[센서] " + label + ": " + detail;
    }
    return "[센서] 사용하지 않는 입력입니다.";
  }
  if (s.startsWith("ID_MV:")) return "[슬롯] ID 전압: " + s.substring(6) + "mV";
  if (s.startsWith("ACCEL:")) return "[가속도] " + s.substring(6) + "m/s² (-1은 사용 불가)";
  if (s.startsWith("DISTANCE:")) return "[장애물 거리] " + s.substring(9) + "cm (-1은 유효 거리 없음)";
  if (s.startsWith("STAIR_DISTANCE:")) return "[계단 거리] " + s.substring(15) + "cm (-1은 사용 불가)";
  if (s.startsWith("ULTRASONIC:")) return "[초음파 거리] " + s.substring(11) + "cm (-1은 유효 거리 없음)";
  if (s.startsWith("MPU_MAX_GAP_MS:")) return "[가속도] 최대 측정 갱신 간격: " + s.substring(15) + "ms";
  if (s.startsWith("PROFILE,")) return "[사용자] " + eventField(s,1) + " / 장애물 " + eventField(s,2) + "cm / 키 코드 " + eventField(s,3) + " / 진동 " + eventField(s,4);
  if (s == "SYSTEM,SENSOR_TASK_FAILED") return "[시스템 오류] 센서 처리 태스크를 만들지 못했습니다. 메모리와 부팅 상태를 확인하세요.";
  return "[기기 알림] " + s;
}
}
`````

## 파일: src/ble_link.cpp

SHA-256: `5cfd6d872fc3798c13134a74a1afe79a136fed2db1a56fcb46d512173748f27a`

`````cpp
#include "ble_link.h"
#include "config.h"
#include "serial_text.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <atomic>
namespace {
constexpr size_t MAX_LINE = 96;
struct Line { char text[MAX_LINE+1]; uint32_t epoch; };
QueueHandle_t rxQueue = nullptr, txQueue = nullptr;
BLEServer *server = nullptr;
BLECharacteristic *characteristic = nullptr;
BLE2902 *cccd = nullptr;
std::atomic<bool> connected{false}, syncPending{false}, disconnected{false};
std::atomic<uint16_t> mtu{23};
std::atomic<bool> streamMode{false};
portMUX_TYPE fallMux = portMUX_INITIALIZER_UNLOCKED;
bool fallPending = false;
uint32_t fallAt = 0;
std::atomic<uint32_t> rxEpoch{0};
uint32_t advertiseAt = 0;
void queueCommand(const String &s) {
  if (!s.length()) return;
  Line line = {}; line.epoch=rxEpoch.load(); s.toCharArray(line.text, sizeof(line.text));
  if (xQueueSend(rxQueue, &line, 0) != pdTRUE) sendMessage("ERR,QUEUE,FULL");
}
bool legacyWhole(const String &s) {
  return s == "STREAM_ON" || s == "PING" || s == "REG" || s == "REG_CANCEL" || s == "GET_STATUS" ||
         s == "GET_ACCEL" || s == "GET_DISTANCE" || s == "GET_STAIR_DISTANCE" ||
         s == "GET_SENSOR" || s == "LED_ON" || s == "LED_OFF" || s == "LED_STATUS" || s == "LED_TEST";
}
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { ++rxEpoch; mtu = 23; streamMode=false; connected = true; }
  void onDisconnect(BLEServer *) override {
    connected = false; syncPending = false; disconnected = true; mtu = 23; streamMode=false; ++rxEpoch;
    cccd->setNotifications(false);
  }
  void onMtuChanged(BLEServer *, esp_ble_gatts_cb_param_t *param) override {
    mtu = param->mtu.mtu;
  }
};
class CccdCallbacks : public BLEDescriptorCallbacks {
  void onWrite(BLEDescriptor *) override {
    if (cccd->getNotifications()) syncPending = true;
  }
};
class RxCallbacks : public BLECharacteristicCallbacks {
  String buffer;
  bool discard = false;
  uint32_t lastRx = 0, epoch = 0;
  void onWrite(BLECharacteristic *c) override {
    if (epoch != rxEpoch.load()) { epoch = rxEpoch.load(); buffer = ""; discard = false; }
    if (uint32_t(millis()-lastRx) > 1000) { buffer = ""; discard = false; }
    lastRx = millis();
    const std::string chunk = c->getValue();
    for (char ch : chunk) {
      if (ch == '\r') continue;
      if (ch == '\n') {
        if (discard) sendMessage("ERR,COMMAND,TOO_LONG");
        else { buffer.trim(); queueCommand(buffer); }
        buffer = ""; discard = false;
      } else if (!discard) {
        if (ch == '\0') { discard = true; buffer = ""; }
        else { buffer += ch; if (buffer.length() > MAX_LINE) { discard = true; buffer = ""; } }
      }
    }
    if (!discard && !streamMode && legacyWhole(buffer)) { queueCommand(buffer); buffer = ""; }
  }
};
}
void bleBegin() {
  rxQueue = xQueueCreate(12, sizeof(Line)); txQueue = xQueueCreate(32, sizeof(Line));
  if (!rxQueue || !txQueue) { Serial.println("[휴대폰 오류] 통신 대기열 메모리를 확보하지 못했습니다."); return; }
  BLEDevice::init(cfg::DEVICE_NAME); BLEDevice::setMTU(185);
  server = BLEDevice::createServer(); server->setCallbacks(new ServerCallbacks);
  auto service = server->createService(cfg::SERVICE_UUID);
  characteristic = service->createCharacteristic(cfg::CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_NOTIFY);
  characteristic->setCallbacks(new RxCallbacks);
  cccd = new BLE2902; cccd->setCallbacks(new CccdCallbacks); characteristic->addDescriptor(cccd);
  characteristic->setValue("CONNECTED\n"); service->start();
  auto advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(cfg::SERVICE_UUID); advertising->setScanResponse(true); advertising->start();
}
void sendMessage(const String &s) {
  Serial.println(nav::serialEventText(s));
  if (cfg::SERIAL_RAW_PROTOCOL) Serial.println("[통신 원문] " + s);
  if (s == "FALL_DETECTED") {
    portENTER_CRITICAL(&fallMux);
    fallAt = millis(); fallPending = true;
    portEXIT_CRITICAL(&fallMux);
    return;
  }
  if (!connected || !txQueue || !cccd || !cccd->getNotifications()) return;
  if (s.length() > MAX_LINE-1) { Serial.println("[휴대폰 오류] 전송 메시지가 너무 깁니다. 전송을 취소합니다."); return; }
  Line line = {}; line.epoch=rxEpoch.load(); String framed = s;
  if (!framed.endsWith("\n")) framed += '\n';
  framed.toCharArray(line.text, sizeof(line.text));
  if (xQueueSend(txQueue, &line, 0) != pdTRUE) Serial.println("[휴대폰 경고] 알림 대기열이 가득 찼습니다. GET_STATUS로 현재 상태를 다시 확인하세요.");
}
void sendProtocolMessage(const String &s) { sendMessage(s); }
bool blePopCommand(String &s) {
  Line line;
  while (rxQueue && xQueueReceive(rxQueue,&line,0)==pdTRUE) {
    if (connected && line.epoch==rxEpoch.load()) { s=line.text; return true; }
  }
  return false;
}
bool bleConnected() { return connected; }
uint32_t bleSessionEpoch() { return rxEpoch.load(); }
void bleEnableStream() { streamMode=true; }
bool bleTakeSync() { return syncPending.exchange(false); }
void bleTick() {
  static Line current={};
  static size_t offset=0;
  static uint32_t lastSend=0, heldAt=0, warnedEpoch=UINT32_MAX;
  // Expire before the connection/CCCD checks. An old event must not survive offline.
  bool expired=false;
  portENTER_CRITICAL(&fallMux);
  if (fallPending && uint32_t(millis()-fallAt)>=cfg::FALL_PENDING_MAX_MS) {
    fallPending=false; expired=true;
  }
  portEXIT_CRITICAL(&fallMux);
  if (expired) Serial.println("[낙상 알림] 보관한 낙상이 5분을 지나 만료됐습니다. 오래된 사건은 재전송하지 않습니다.");
  if (disconnected.exchange(false)) {
    advertiseAt=millis(); current.text[0]='\0'; offset=0;
    // Epoch tags reject stale commands/messages even if a rapid reconnect happened.
  }
  if (!connected && advertiseAt && uint32_t(millis()-advertiseAt)>=500) {
    server->startAdvertising(); advertiseAt=0;
  }
  if (!connected || !cccd || !cccd->getNotifications() || uint32_t(millis()-lastSend)<12) return;
  size_t payload=mtu.load()>3 ? mtu.load()-3 : 20;
  auto notify=[&](const char *line) {
    characteristic->setValue(reinterpret_cast<uint8_t *>(const_cast<char *>(line)),strlen(line));
    characteristic->notify(); lastSend=millis();
  };
  // This event fits even MTU 23. Finish an existing stream frame before taking it.
  bool takeFall=false;
  if (offset==0) {
    portENTER_CRITICAL(&fallMux);
    if (fallPending) {
      takeFall=uint32_t(millis()-fallAt)<cfg::FALL_PENDING_MAX_MS;
      fallPending=false;
    }
    portEXIT_CRITICAL(&fallMux);
  }
  if (takeFall) { notify("FALL_DETECTED\n"); return; }
  if (current.text[0] && current.epoch!=rxEpoch.load()) { current.text[0]='\0'; offset=0; }
  while (!current.text[0]) {
    if (xQueueReceive(txQueue,&current,0)!=pdTRUE) return;
    if (current.epoch!=rxEpoch.load()) { current.text[0]='\0'; continue; }
    heldAt=millis(); offset=0;
  }
  if (!streamMode && strlen(current.text)>payload) {
    // The original Android app treats one notify as one event. Never send fragments.
    if (warnedEpoch!=rxEpoch.load()) {
      warnedEpoch=rxEpoch.load(); notify("ERROR:MTU_LOW\n");
    }
    if (uint32_t(millis()-heldAt)<2000) return; // give MTU negotiation time
    Serial.println("[휴대폰 경고] 전송 크기 협상이 지연되어 긴 메시지를 건너뜁니다. MTU 또는 STREAM_ON과 GET_STATUS를 확인하세요.");
    current.text[0]='\0'; // do not let an oversized line block all short replies
    return;
  }
  size_t length=strlen(current.text), count=min(payload,length-offset);
  characteristic->setValue(reinterpret_cast<uint8_t *>(current.text+offset),count);
  characteristic->notify(); lastSend=millis(); offset+=count;
  if (offset==length) { current.text[0]='\0'; offset=0; }
}
`````

## 파일: src/expansion.cpp

SHA-256: `2d3ecd64fb01f5b01af8ec86ff3ce4145b9992ff40ec55cc9b5523688b71742d`

`````cpp
#include "expansion.h"
#include "config.h"
#include "ble_link.h"
#include "profiles.h"
#include "feedback.h"
#include <SPI.h>
#include <MFRC522.h>
namespace {
nav::SlotGate gate;
nav::Module observed = nav::Module::None;
uint16_t idMv = 3300;
uint32_t lastIdPoll = 0, unknownAt = 0;
bool unknownWarned = false;
bool spiRunning = false, faulted = false, suspended = false;
nav::UpperDecision upper;
nav::EchoResult echoResult = nav::EchoResult::NoResponse;
MFRC522 rfid(cfg::S4, UINT8_MAX); // RST tied to MODULE 3.3V, no GPIO15
// Two retries, then latch FAULT until the module is physically removed.
enum class RfidState { Off, Resetting, Ready, Fault };
RfidState rfidState = RfidState::Off;
uint32_t resetAt = 0, healthAt = 0, cardAt = 0;
uint8_t retries = 0, healthMiss = 0, cardMiss = 0;
bool cardPresent = false;
uint8_t heldUid[10] = {}, heldUidLen = 0;
uint32_t remoteOffAt = 0;
float ultrasonicCm = -1;
uint32_t ultrasonicAt = 0, ultrasonicValidAt = 0, triggerAt = 0, triggerUs = 0;
uint8_t echoMiss = 0;
bool echoWaiting = false, echoInterruptAttached = false;
portMUX_TYPE echoMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool echoArmed = false, echoSawRise = false, echoDone = false;
volatile uint32_t echoRiseUs = 0, echoWidthUs = 0;
uint32_t ledTestAt = 0, ledAt = 0;
bool ledTesting = false;
uint8_t lastLedMask = 0xFF;

void IRAM_ATTR echoIsr() {
  portENTER_CRITICAL_ISR(&echoMux);
  if (echoArmed) {
    if (digitalRead(cfg::S3)) { echoRiseUs = micros(); echoSawRise = true; }
    else if (echoSawRise) {
      echoWidthUs = uint32_t(micros()-echoRiseUs); echoDone = true; echoArmed = false;
    }
  }
  portEXIT_CRITICAL_ISR(&echoMux);
}
void highZ(bool forget = true) {
  if (echoInterruptAttached) { detachInterrupt(digitalPinToInterrupt(cfg::S3)); echoInterruptAttached=false; }
  portENTER_CRITICAL(&echoMux); echoArmed = echoDone = false; portEXIT_CRITICAL(&echoMux);
  if (spiRunning) { SPI.end(); spiRunning = false; }
  pinMode(cfg::S1, INPUT); pinMode(cfg::S2, INPUT); pinMode(cfg::S3, INPUT); pinMode(cfg::S4, INPUT);
  echoWaiting = false; remoteOffAt = 0;
  if (forget) rfidState = RfidState::Off;
}
void fault(const char *message, bool latchRfid) {
  if (!faulted) { sendMessage(message); feedbackStart(Feedback::Fault); }
  faulted = true;
  if (latchRfid) {
    profilesCancelRegistration("RFID_FAULT"); highZ(); rfidState = RfidState::Fault;
    sendMessage("MODULE,RFID,FAULT,REATTACH");
  }
}
bool versionValid(uint8_t v) {
  return v == 0x91 || v == 0x92 || v == 0x88 || v == 0x90 || v == 0x12 || v == 0xB2;
}
void softReset() {
  rfid.PCD_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
  rfidState = RfidState::Resetting; resetAt = millis(); cardPresent = false;
}
void configureRfid() {
  rfid.PCD_WriteRegister(MFRC522::TxModeReg,0);
  rfid.PCD_WriteRegister(MFRC522::RxModeReg,0);
  rfid.PCD_WriteRegister(MFRC522::ModWidthReg,0x26);
  rfid.PCD_WriteRegister(MFRC522::TModeReg,0x80);
  rfid.PCD_WriteRegister(MFRC522::TPrescalerReg,0xA9);
  rfid.PCD_WriteRegister(MFRC522::TReloadRegH,0);
  rfid.PCD_WriteRegister(MFRC522::TReloadRegL,0x64); // 2.5 ms RF timeout
  rfid.PCD_WriteRegister(MFRC522::TxASKReg,0x40);
  rfid.PCD_WriteRegister(MFRC522::ModeReg,0x3D);
  rfid.PCD_AntennaOn();
}
void activate(nav::Module type) {
  faulted = false; retries = healthMiss = cardMiss = 0;
  heldUidLen = 0; cardPresent = false; ultrasonicCm = -1; echoMiss = 0; upper.reset();
  if (type == nav::Module::Rfid) {
    digitalWrite(cfg::S4,HIGH); pinMode(cfg::S4,OUTPUT);
    SPI.begin(cfg::S1,cfg::S3,cfg::S2,cfg::S4); spiRunning = true;
    softReset(); sendMessage("RFID 연결"); sendMessage("MODULE,RFID,INITIALIZING");
  } else if (type == nav::Module::Remote) {
    digitalWrite(cfg::S1,LOW); pinMode(cfg::S1,OUTPUT);
    digitalWrite(cfg::S2,LOW); pinMode(cfg::S2,OUTPUT);
    pinMode(cfg::S3,INPUT); attachInterrupt(digitalPinToInterrupt(cfg::S3),echoIsr,CHANGE); echoInterruptAttached=true;
    triggerAt = millis()-cfg::ULTRASONIC_PERIOD_MS;
    sendMessage("리모컨 연결"); sendMessage("MODULE,REMOTE,READY");
  } else if (type == nav::Module::Led) {
    digitalWrite(cfg::S1,LOW); pinMode(cfg::S1,OUTPUT);
    digitalWrite(cfg::S2,LOW); pinMode(cfg::S2,OUTPUT);
    digitalWrite(cfg::S4,LOW); pinMode(cfg::S4,OUTPUT);
    lastLedMask = 0xFF; ledTesting = false; ledAt = millis()-100;
    sendMessage("LED 연결"); sendMessage("MODULE,LED,READY");
  }
}
void rfidTick() {
  if (rfidState == RfidState::Fault || rfidState == RfidState::Off) return;
  const uint32_t now = millis();
  if (rfidState == RfidState::Resetting) {
    if (!nav::elapsed(now,resetAt,60)) return;
    const uint8_t version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    bool ok = versionValid(version) && !(rfid.PCD_ReadRegister(MFRC522::CommandReg)&0x10);
    if (ok) {
      configureRfid();
      ok = rfid.PCD_ReadRegister(MFRC522::TxASKReg) == 0x40 &&
           (rfid.PCD_ReadRegister(MFRC522::TxControlReg)&3) == 3;
    }
    if (!ok) {
      if (retries++ < 2) softReset();
      else fault("RFID 응답 없음",true);
    } else {
      rfidState = RfidState::Ready; healthMiss = 0; retries = 0; healthAt = now;
      sendMessage("MODULE,RFID,READY");
    }
    return;
  }
  if (nav::elapsed(now,healthAt,1000)) {
    healthAt = now;
    bool ok = versionValid(rfid.PCD_ReadRegister(MFRC522::VersionReg)) &&
              rfid.PCD_ReadRegister(MFRC522::TxASKReg) == 0x40;
    if (ok) healthMiss = 0;
    else if (++healthMiss >= 2) { softReset(); return; }
  }
  if (!nav::elapsed(now,cardAt,150)) return;
  cardAt = now;
  byte atqa[2], size = sizeof(atqa);
  auto status = rfid.PICC_WakeupA(atqa,&size);
  if (status != MFRC522::STATUS_OK && status != MFRC522::STATUS_COLLISION) {
    if (cardMiss < 3) ++cardMiss;
    if (cardMiss >= 3) { cardPresent = false; heldUidLen = 0; }
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) return;
  const uint8_t len = rfid.uid.size;
  bool valid = len == 4 || len == 7 || len == 10;
  bool different = valid && (!cardPresent || len != heldUidLen || memcmp(heldUid,rfid.uid.uidByte,len) != 0);
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
  if (valid) {
    if (different || profilesWaiting()) profilesCard(rfid.uid.uidByte,len);
    memcpy(heldUid,rfid.uid.uidByte,len); heldUidLen = len;
    cardPresent = true; cardMiss = 0;
  }
}
void recordEcho(nav::EchoResult result, uint32_t width, uint32_t now) {
  echoResult=result; ultrasonicAt=now;
  const bool responding=result==nav::EchoResult::Distance || result==nav::EchoResult::Far;
  ultrasonicCm=result==nav::EchoResult::Distance ? width*0.0343f/2 : -1;
  if (responding) {
    ultrasonicValidAt=now;
    echoMiss=0;
    if (faulted) { faulted=false; sendMessage("SENSOR:ULTRASONIC:READY"); }
    upper.sample(ultrasonicCm);
  } else {
    ultrasonicCm=-1; upper.invalid();
    if (echoMiss<cfg::ULTRASONIC_MISS_LIMIT) ++echoMiss;
    if (echoMiss>=cfg::ULTRASONIC_MISS_LIMIT) {
      upper.reset();
      fault(result==nav::EchoResult::StuckHigh ? "ERROR:ULTRASONIC_STUCK_HIGH" : "ERROR:ULTRASONIC_NO_RESPONSE",false);
    }
  }
}
void ultrasonicTick() {
  const uint32_t now=millis();
  if (remoteOffAt && int32_t(now-remoteOffAt)>=0) { digitalWrite(cfg::S2,LOW); remoteOffAt=0; }
  bool done, sawRise; uint32_t width;
  portENTER_CRITICAL(&echoMux);
  done=echoDone; sawRise=echoSawRise; width=echoWidthUs; echoDone=false;
  portEXIT_CRITICAL(&echoMux);
  if (echoWaiting && done) {
    echoWaiting=false;
    recordEcho(nav::classifyEcho(true,sawRise,false,width),width,now);
  }
  if (echoWaiting && uint32_t(micros()-triggerUs)>=cfg::ULTRASONIC_TIMEOUT_US) {
    // Take one atomic snapshot at timeout so a just-completed falling edge is not lost.
    portENTER_CRITICAL(&echoMux);
    done=echoDone; sawRise=echoSawRise; width=echoWidthUs;
    echoDone=false; echoArmed=false;
    portEXIT_CRITICAL(&echoMux);
    echoWaiting=false;
    recordEcho(nav::classifyEcho(done,sawRise,digitalRead(cfg::S3),width),width,now);
  }
  if (!echoWaiting && nav::elapsed(now,triggerAt,cfg::ULTRASONIC_PERIOD_MS)) {
    triggerAt=now;
    // A pre-existing HIGH is an abnormal input, never a valid far-distance response.
    if (digitalRead(cfg::S3)) { recordEcho(nav::EchoResult::StuckHigh,0,now); return; }
    triggerUs=micros(); echoWaiting=true;
    portENTER_CRITICAL(&echoMux);
    echoSawRise=echoDone=false; echoArmed=true;
    portEXIT_CRITICAL(&echoMux);
    digitalWrite(cfg::S1,HIGH); delayMicroseconds(10); digitalWrite(cfg::S1,LOW);
  }
}
// Resume a short contact disturbance without resetting the RC522 or logging a detach.
void resume(nav::Module type) {
  if (type==nav::Module::Rfid && rfidState!=RfidState::Fault) {
    digitalWrite(cfg::S4,HIGH); pinMode(cfg::S4,OUTPUT);
    SPI.begin(cfg::S1,cfg::S3,cfg::S2,cfg::S4); spiRunning=true;
    healthAt=millis()-1000; // verify existing configuration before card use
  } else if (type==nav::Module::Remote) {
    digitalWrite(cfg::S1,LOW); pinMode(cfg::S1,OUTPUT);
    digitalWrite(cfg::S2,LOW); pinMode(cfg::S2,OUTPUT);
    pinMode(cfg::S3,INPUT); attachInterrupt(digitalPinToInterrupt(cfg::S3),echoIsr,CHANGE); echoInterruptAttached=true;
    triggerAt=millis()-cfg::ULTRASONIC_PERIOD_MS;
  } else if (type==nav::Module::Led) {
    digitalWrite(cfg::S1,LOW); pinMode(cfg::S1,OUTPUT);
    digitalWrite(cfg::S2,LOW); pinMode(cfg::S2,OUTPUT);
    digitalWrite(cfg::S4,LOW); pinMode(cfg::S4,OUTPUT); lastLedMask=0xFF;
  }
}
void ledTick(uint8_t mask) {
  if (ledTesting) {
    uint32_t elapsed = millis()-ledTestAt;
    if (elapsed >= 3000) ledTesting = false;
    else mask = 1 << ((elapsed/500)%5);
  }
  if (mask == lastLedMask || !nav::elapsed(millis(),ledAt,100)) return;
  ledAt = millis(); lastLedMask = mask;
  digitalWrite(cfg::S4,LOW); shiftOut(cfg::S2,cfg::S1,MSBFIRST,mask&0x1F); digitalWrite(cfg::S4,HIGH);
}
}
void expansionBegin() {
  highZ(); pinMode(cfg::SLOT_ID,INPUT); analogSetPinAttenuation(cfg::SLOT_ID,ADC_11db);
}
void expansionTick(uint8_t mask) {
  if (nav::elapsed(millis(),lastIdPoll,cfg::ID_POLL_MS)) {
    lastIdPoll = millis(); idMv = analogReadMilliVolts(cfg::SLOT_ID); observed = nav::identify(idMv);
    if (observed == nav::Module::Unknown) {
      if (!unknownAt) unknownAt = millis();
      if (!unknownWarned && nav::elapsed(millis(),unknownAt,300)) {
        unknownWarned = true; sendMessage("MODULE,UNKNOWN,ID_INVALID"); feedbackStart(Feedback::Fault);
      }
    } else { unknownAt = 0; unknownWarned = false; }
    const auto previous=gate.active;
    const bool wasPermitted=gate.permitted;
    if (gate.update(observed,millis())) {
      highZ(); suspended=false; upper.reset(); ultrasonicCm=-1;
      if (previous!=nav::Module::None) {
        profilesCancelRegistration("DETACHED"); sendMessage("확장모듈 분리");
      }
      faulted=false;
      if (gate.active!=nav::Module::None) activate(gate.active);
    } else if (wasPermitted && !gate.permitted) {
      highZ(false); suspended=true; upper.reset(); ultrasonicCm=-1;
    } else if (suspended && gate.permitted) {
      resume(gate.active); suspended=false;
    }

  }
  if (!gate.permitted) return;
  if (nav::elapsed(millis(),ultrasonicValidAt,600)) upper.reset();
  if (gate.active == nav::Module::Rfid) rfidTick();
  else if (gate.active == nav::Module::Remote) ultrasonicTick();
  else if (gate.active == nav::Module::Led) ledTick(mask);
}
void expansionRemotePress() {
  if (!gate.permitted || gate.active != nav::Module::Remote) return;
  digitalWrite(cfg::S2,HIGH); remoteOffAt = millis()+cfg::REMOTE_PULSE_MS;
  sendMessage("REMOTE,TRIGGERED");
}
void expansionLedTest() {
  if (!gate.permitted || gate.active != nav::Module::Led) { sendMessage("ERR,LED,NO_MODULE"); return; }
  ledTesting = true; ledTestAt = millis(); sendMessage("LED,TEST");
}
bool expansionRfidReady() { return gate.permitted && gate.active == nav::Module::Rfid && rfidState == RfidState::Ready; }
nav::Module expansionModule() {
  return gate.active == nav::Module::None && observed == nav::Module::Unknown ? nav::Module::Unknown : gate.active;
}
uint16_t expansionIdMv() { return idMv; }
float expansionUltrasonicCm() {
  return gate.permitted && gate.active == nav::Module::Remote && !nav::elapsed(millis(),ultrasonicAt,600) ? ultrasonicCm : -1;
}
bool expansionFault() { return faulted || unknownWarned; }

bool expansionUpperAlert() {
  return gate.permitted && gate.active==nav::Module::Remote && upper.alert &&
         !nav::elapsed(millis(),ultrasonicValidAt,600);
}
float expansionUpperDistance() { return expansionUpperAlert() ? upper.warningCm : -1; }
const char *expansionEchoState() {
  if (!gate.permitted || gate.active!=nav::Module::Remote) return "ABSENT";
  switch (echoResult) {
    case nav::EchoResult::Distance: return "DISTANCE";
    case nav::EchoResult::Far: return "FAR";
    case nav::EchoResult::StuckHigh: return "STUCK_HIGH";
    case nav::EchoResult::Invalid: return "INVALID";
    default: return "NO_RESPONSE";
  }
}
`````

## 파일: src/feedback.cpp

SHA-256: `4aa50d1bbf4866fb3d08bec2800cb37dab1636326e09241c7f596da8044373fc`

`````cpp
#include "feedback.h"
#include "config.h"
#include "profiles.h"
namespace {
const uint16_t green[] = {150,150,500,150,150,150,500};
const uint16_t applied[] = {120,120,120}, unregistered[] = {800};
const uint16_t registered[] = {500,200,500}, failed[] = {100,100,100,100,100,100,100};
const uint16_t fall[] = {700,200,700,200,700};
const uint16_t *pattern = nullptr;
uint8_t count = 0, patternStep = 0, priority = 0;
uint32_t at = 0;
bool servoState = false;
void motor(uint8_t channel, bool on) { ledcWrite(channel, on ? activeVibDuty : 0); }
void angle(int degrees) {
  uint32_t us = map(constrain(degrees,0,180),0,180,cfg::SERVO_MIN_US,cfg::SERVO_MAX_US);
  ledcWrite(cfg::SERVO_CHANNEL, us*65535UL/20000UL);
}
void sideMotor(uint8_t channel, float cm) {
  if (cm < 0 || cm >= activeObstacleThresholdCM) { motor(channel,false); return; }
  uint32_t period = 60 + uint32_t(constrain(cm/activeObstacleThresholdCM,0.0f,1.0f)*440);
  motor(channel, millis()%period < period/2);
}
}
void feedbackBegin() {
  const uint8_t pins[] = {cfg::VIB_MAIN,cfg::VIB_LEFT,cfg::VIB_RIGHT};
  const uint8_t channels[] = {cfg::VIB_CH_MAIN,cfg::VIB_CH_LEFT,cfg::VIB_CH_RIGHT};
  for (int i = 0; i < 3; ++i) {
    ledcSetup(channels[i],5000,8); ledcAttachPin(pins[i],channels[i]); ledcWrite(channels[i],0);
  }
  ledcSetup(cfg::SERVO_CHANNEL,50,16); ledcAttachPin(cfg::SERVO,cfg::SERVO_CHANNEL);
  angle(cfg::SERVO_RELEASE_DEG);
}
void feedbackStart(Feedback type) {
  // Faults remain visible in logs/BLE, without starting or interrupting vibration.
  if (type == Feedback::Fault) {
    Serial.println("[고장 경고] 진동 없이 로그와 앱 상태로 알립니다. 센서·슬롯 원인 로그를 확인하세요.");
    return;
  }
  uint8_t p = type == Feedback::Fall ? 3 : 1;
  if (pattern && priority > p) return;
  priority = p;
#define SELECT(a) pattern=a; count=sizeof(a)/sizeof(a[0]); break
  switch (type) {
    case Feedback::Green: SELECT(green);
    case Feedback::Applied: SELECT(applied);
    case Feedback::Unregistered: SELECT(unregistered);
    case Feedback::Registered: SELECT(registered);
    case Feedback::Failed: SELECT(failed);
    case Feedback::Fault: return;
    case Feedback::Fall: SELECT(fall);
  }
#undef SELECT
  patternStep = 0; at = millis();
  const char *reason=type==Feedback::Fall ? "낙상 판정: 긴 진동 3회" :
                     type==Feedback::Green ? "초록불 감지" :
                     type==Feedback::Applied ? "카드 사용자 설정 적용" :
                     type==Feedback::Registered ? "카드 등록 성공" :
                     type==Feedback::Unregistered ? "미등록 카드" : "카드 처리 실패";
  Serial.println(String("[중앙 진동] ")+reason);
}
void feedbackTick(bool stairAlert, float left, float right, float upperCm) {
  // Upper obstacle drives both side motors; scale its distance to the lower threshold.
  if (upperCm>=0) {
    float scaled=upperCm*activeObstacleThresholdCM/cfg::UPPER_RELEASE_CM;
    scaled=min(scaled,activeObstacleThresholdCM-0.1f);
    left=left<0 ? scaled : min(left,scaled); right=right<0 ? scaled : min(right,scaled);
  }
  sideMotor(cfg::VIB_CH_LEFT,left); sideMotor(cfg::VIB_CH_RIGHT,right);
  // FALL is never cancelled by stair caution. Brake control is independent.
  if (stairAlert && priority<3) { pattern=nullptr; priority=0; motor(cfg::VIB_CH_MAIN,true); return; }
  if (pattern && uint32_t(millis()-at) >= pattern[patternStep]) {
    at = millis(); if (++patternStep >= count) { pattern = nullptr; priority = 0; }
  }
  if (pattern) motor(cfg::VIB_CH_MAIN,patternStep%2 == 0);
  else motor(cfg::VIB_CH_MAIN,stairAlert);
}
void brakeSet(bool active) {
  if (active != servoState) { servoState = active; angle(active ? cfg::SERVO_APPLY_DEG : cfg::SERVO_RELEASE_DEG); }
}
`````

## 파일: src/main.cpp

SHA-256: `71b0b0fd70ee76675347ea2acdec28e08a855c7510e453f2834e7d5e94f12597`

`````cpp
#include <Arduino.h>
#include "config.h"
#include "logic.h"
#include "sensors.h"
#include "ble_link.h"
#include "profiles.h"
#include "expansion.h"
#include "feedback.h"
namespace {
SensorSnapshot sensors;
nav::StairDecision stairDecision;
uint32_t lastStairSeq = 0, lastFallCount = 0;
uint8_t obstacleSide = 0, sentObstacle = 0xFF;
uint32_t obstacleEventAt = 0;
bool stairAlert = false, sentStair = false;
bool sentFront = false;
bool pogoReported=false, pogoCandidate=false; uint32_t pogoAt=0;
String serialBuffer;
bool serialDiscard=false;
uint32_t previousBleSession=0;
uint32_t frontEventAt = 0;
bool yoloOn = false;
bool lastButton = HIGH, stableButton = HIGH;
uint32_t buttonAt = 0, printAt = 0, healthAt = 0;
uint8_t previousHealth = 0xFF;
bool mpuUsable = false, stairUsable = false, obstacleUsable = false;
float leftCm = -1, rightCm = -1;
String piBuffer;
bool piDiscard = false;
uint32_t piRxAt = 0;
const char *moduleName(nav::Module m) {
  switch (m) {
    case nav::Module::Rfid: return "RFID";
    case nav::Module::Remote: return "REMOTE";
    case nav::Module::Led: return "LED";
    case nav::Module::Unknown: return "UNKNOWN";
    default: return "NONE";
  }
}
uint8_t faultMask() {
  return (!obstacleUsable ? 1 : 0) | (!stairUsable ? 2 : 0) | (!mpuUsable ? 4 : 0)
         | (expansionFault() ? 8 : 0);
}
uint8_t healthMask() {
  // All outputs mean normal/active: Q0 L5CX, Q1 MPU, Q2 L4CX, Q3 BLE, Q4 YOLO.
  return nav::normalLeds(obstacleUsable,mpuUsable,stairUsable,bleConnected(),yoloOn);
}
void status() {
  if (pogoReported) sendMessage("포고핀 재결합");
  else if (sensors.mpuConnected || sensors.stairConnected) sendMessage("포고핀 연결확인 중");
  else sendMessage("포고핀 분리상태");
  sendMessage("SENSOR:MPU:"+String(mpuUsable ? "READY" : "FAULT"));
  sendMessage("SENSOR:STAIR:"+String(stairUsable ? "READY" : "FAULT"));
  sendMessage("SENSOR:OBSTACLE:"+String(obstacleUsable ? "READY" : "FAULT"));
  sendMessage("SENSOR:STAIR_ALERT:"+String(stairAlert ? 1 : 0));
  sendMessage(String("MODULE,")+moduleName(expansionModule())+","+(expansionFault() ? "FAULT" : "ATTACHED"));
  sendMessage("ID_MV:"+String(expansionIdMv()));
  sendMessage("PROFILE,"+String(activeProfileName)+","+String(activeObstacleThresholdCM,0)+","+
              String(activeStairThresholdCM,0)+","+String(activeVibDuty));
  sendMessage("YOLO:"+String(yoloOn ? "ON" : "OFF"));
  sendMessage(String("SENSOR:PROFILE_STORE:")+(profilesStoreReady() ? "READY" : "FAULT"));
  sendMessage(String("SENSOR:ULTRASONIC:")+expansionEchoState());
  sendMessage(expansionUpperAlert() ? "OBSTACLE:UPPER" : "OBSTACLE:UPPER_CLEAR");
  if (stairAlert) sendMessage("계단 감지");
  else sendMessage("STAIR_CLEARED");
  sendMessage("MPU_MAX_GAP_MS:"+String(sensors.maxAccelGapMs));
  sentObstacle = 0xFF; obstacleEventAt = millis()-300;
}
void processCommand(const String &c) {
  if (profilesCommand(c)) return;
  if (c == "STREAM_ON") { bleEnableStream(); sendMessage("ACK,STREAM"); }
  else if (c == "PING") sendMessage("PONG");
  else if (c == "GET_STATUS") status();
  else if (c == "GET_ACCEL") sendMessage("ACCEL:"+String(mpuUsable ? sensors.accel : -1,2));
  else if (c == "GET_DISTANCE") {
    float cm = leftCm < 0 ? rightCm : rightCm < 0 ? leftCm : min(leftCm,rightCm);
    sendMessage("DISTANCE:"+String(cm,1));
  }
  else if (c == "GET_STAIR_DISTANCE") sendMessage("STAIR_DISTANCE:"+String(stairUsable ? sensors.stairCm : -1,1));
  else if (c == "GET_ULTRASONIC") sendMessage("ULTRASONIC:"+String(expansionUltrasonicCm(),1));
  else if (c == "LED_TEST" || c == "LED_ON") expansionLedTest();
  else if (c == "LED_STATUS") sendMessage(String("LED:")+(expansionModule()==nav::Module::Led ? "MODULE" : "ABSENT"));
  else if (c == "GET_SENSOR") sendMessage("SENSOR:UNAVAILABLE"); // GPIO34 is reserved
  else if (c == "LED_OFF") sendMessage("LED,AUTO_HEALTH");
  else sendMessage("ERROR:UNKNOWN_COMMAND");
}
void checkButton() {
  bool raw = digitalRead(cfg::BUTTON); uint32_t now = millis();
  if (raw != lastButton) { lastButton = raw; buttonAt = now; }
  if (nav::elapsed(now,buttonAt,50) && raw != stableButton) {
    stableButton = raw;
    if (raw == LOW) {
      Serial2.print("BTN\n"); expansionRemotePress(); sendMessage("BUTTON_PRESSED");
    }
  }
}
void checkPi() {
  uint32_t now = millis();
  if (piBuffer.length() && nav::elapsed(now,piRxAt,1000)) { piBuffer=""; piDiscard=false; }
  for (int n = 0; n < 64 && Serial2.available(); ++n) {
    char c = Serial2.read(); piRxAt = now;
    if (c == '\r') continue;
    if (c == '\n') {
      if (!piDiscard) {
        if (piBuffer == "GREEN") { feedbackStart(Feedback::Green); sendMessage("GREEN_LIGHT"); }
        else if (piBuffer == "YOLO_ON") { yoloOn = true; sendMessage("YOLO:ON"); }
        else if (piBuffer == "YOLO_OFF") { yoloOn = false; sendMessage("YOLO:OFF"); }
      }
      piBuffer=""; piDiscard=false;
    } else if (!piDiscard) {
      if ((c >= 'A' && c <= 'Z') || c == '_') {
        piBuffer+=c; if (piBuffer.length()>24) { piBuffer=""; piDiscard=true; }
      } else { piBuffer=""; piDiscard=true; }
    }
  }
}
const char *mpuStatusText() {
  if (mpuUsable) return "정상";
  if (sensors.mpuReady && !sensors.accelSeq) return "첫 측정 대기";
  if (sensors.mpuReady) return "측정값 갱신 지연 (120ms 이상, 낙상 판정에 사용하지 않음)";
  return sensors.mpuReason;
}
const char *stairStatusText() {
  if (stairUsable) return sensors.stairNoTarget ? "정상 통신·반사 대상 없음" : "정상";
  if (sensors.stairReady && !sensors.stairSeq) return "첫 측정 대기";
  if (sensors.stairReady) return "측정값 갱신 지연 (600ms 이상)";
  return sensors.stairReason;
}
const char *obstacleStatusText() {
  if (obstacleUsable) return "정상";
  if (sensors.obstacleReady && !sensors.obstacleSeq) return "첫 측정 대기";
  if (sensors.obstacleReady) return "측정값 갱신 지연 (600ms 이상, 2.5초 무프레임이면 재초기화)";
  return sensors.obstacleReason;
}
const char *moduleStatusText() {
  switch (expansionModule()) {
    case nav::Module::Rfid: return "RFID 카드";
    case nav::Module::Remote: return "리모컨·초음파";
    case nav::Module::Led: return "LED 표시";
    case nav::Module::Unknown: return "판별 불가: 미구성 슬롯은 GPIO36 외부 풀업 확인";
    default: return "모듈 없음";
  }
}
void consumeSensors() {
  sensors = sensorSnapshot(); uint32_t now = millis();
  mpuUsable = sensors.mpuReady && sensors.accelSeq && !nav::elapsed(now,sensors.accelAt,cfg::ACCEL_STALE_MS);
  stairUsable = sensors.stairReady && sensors.stairSeq && sensors.stairAt && !nav::elapsed(now,sensors.stairAt,cfg::STAIR_STALE_MS);
  obstacleUsable = sensors.obstacleReady && sensors.obstacleSeq && !nav::elapsed(now,sensors.obstacleAt,cfg::OBSTACLE_STALE_MS);
  if (sensors.fallCount != lastFallCount) {
    lastFallCount = sensors.fallCount; sendMessage("FALL_DETECTED"); feedbackStart(Feedback::Fall);
  }
  static float previousThreshold = activeStairThresholdCM;
  if (previousThreshold != activeStairThresholdCM) {
    previousThreshold = activeStairThresholdCM; stairDecision.reset();
  }
  if (stairUsable && sensors.stairSeq != lastStairSeq) {
    lastStairSeq = sensors.stairSeq;
    stairDecision.sample(sensors.stairCm,sensors.stairNoTarget,activeStairThresholdCM);
  }
  if (!stairUsable) {
    stairDecision.reset();
    // Fault warning is separate from stair evidence; servo releases on lost sensor.
  }
  stairAlert = stairDecision.alert;
  if (stairAlert != sentStair) {
    sentStair=stairAlert; sendMessage(stairAlert ? "계단 감지" : "STAIR_CLEARED");
  }
  leftCm = obstacleUsable ? sensors.leftCm : -1;
  rightCm = obstacleUsable ? sensors.rightCm : -1;
  bool left = leftCm >= 0 && leftCm < activeObstacleThresholdCM;
  bool right = rightCm >= 0 && rightCm < activeObstacleThresholdCM;
  obstacleSide = (left ? 1 : 0) | (right ? 2 : 0);
  if (obstacleSide != sentObstacle && nav::elapsed(now,obstacleEventAt,300)) {
    sentObstacle=obstacleSide; obstacleEventAt=now;
    const char *names[] = {"OBSTACLE:NONE","OBSTACLE:LEFT","OBSTACLE:RIGHT","OBSTACLE:BOTH"};
    sendMessage(names[obstacleSide]);
  }
  if (nav::elapsed(now,healthAt,500)) {
    healthAt=now;
    uint8_t mask = faultMask();
    if (mask != previousHealth) {
      // Base sensor transitions request a log-only fault warning; expansion reports separately.
      if (previousHealth != 0xFF && (mask & ~previousHealth & 0x07)) feedbackStart(Feedback::Fault);
      previousHealth = mask;
      sendMessage("SENSOR:MPU:"+String(mpuUsable ? "READY" : "FAULT"));
      sendMessage("SENSOR:STAIR:"+String(stairUsable ? "READY" : "FAULT"));
      sendMessage("SENSOR:OBSTACLE:"+String(obstacleUsable ? "READY" : "FAULT"));
    }
  }
  bool pogoNow=sensors.mpuConnected || sensors.stairConnected;
  if (pogoNow!=pogoCandidate) { pogoCandidate=pogoNow; pogoAt=now; }
  if (pogoNow!=pogoReported && nav::elapsed(now,pogoAt,100) && (!pogoNow || mpuUsable || stairUsable)) {
    pogoReported=pogoNow; sendMessage(pogoNow ? "포고핀 재결합" : "포고핀 분리상태");
  }
  if (nav::elapsed(now,printAt,1000)) {
    printAt=now;
    Serial.printf("[상태] 가속도=%s (%.2f m/s²) | 계단=%s (%.1fcm, 위험=%s) | 장애물=%s (왼쪽 %.1fcm / 오른쪽 %.1fcm)\n",
      mpuStatusText(),mpuUsable ? sensors.accel : -1.0f,stairStatusText(),stairUsable ? sensors.stairCm : -1.0f,
      stairAlert ? "감지" : "없음",obstacleStatusText(),leftCm,rightCm);
    Serial.printf("[슬롯 상태] ID=%umV / %s | 남은 메모리=%u바이트\n",
      expansionIdMv(),moduleStatusText(),ESP.getFreeHeap());
  }
}
}
void setup() {
  Serial.begin(115200);
  expansionBegin(); // shared signals high-Z before sensor initialization
  pinMode(cfg::BUTTON,INPUT_PULLUP);
  lastButton=stableButton=digitalRead(cfg::BUTTON);
  Serial2.begin(115200,SERIAL_8N1,cfg::UART_RX,cfg::UART_TX);
  feedbackBegin(); profilesBegin(); bleBegin();
  if (!sensorsBegin()) { sendMessage("SYSTEM,SENSOR_TASK_FAILED"); feedbackStart(Feedback::Fault); }
  Serial.println("[시작] NAVCANE v4.2 한국어 진단·고장 경고 무진동 버전. 센서 초기화는 별도 태스크에서 진행합니다.");
  Serial.println("[핀 안내] 중앙 진동=32, 왼쪽=14, 오른쪽=26, 버튼=33. GPIO33에 진동 드라이버를 연결하지 마세요.");
  Serial.println("[안내] 미연결 센서는 사용 불가로 표시됩니다. 포고핀 상태는 센서 응답으로 추정합니다. -1 거리는 유효 측정 없음입니다.");
}
void loop() {
  // Clear a previous session even if a disconnect/reconnect occurred between loops.
  uint32_t session=bleSessionEpoch();
  if (session!=previousBleSession) { profilesClearPending(); previousBleSession=session; }
  profilesTick(); // registration deadline and session are checked before RFID polling
  consumeSensors();
  expansionTick(healthMask()); // ID is checked before button or BLE-driven output
  checkButton(); checkPi();
  for (int i=0;i<64 && Serial.available();++i) {
    char c=Serial.read(); if (c=='\r') continue;
    if (c=='\n') {
      if (!serialDiscard) { serialBuffer.trim(); profilesSerialCommand(serialBuffer); }
      serialBuffer=""; serialDiscard=false;
    } else if (!serialDiscard) {
      serialBuffer+=c;
      if (serialBuffer.length()>64) { serialBuffer=""; serialDiscard=true; }
    }
  }
  String command;
  if (bleTakeSync()) { profilesClearPending(); sendMessage("CONNECTED"); status(); }
  for (int n=0;n<4 && blePopCommand(command);++n) processCommand(command);
  profilesTick();
  float frontCm = expansionUpperDistance();
  bool frontObstacle=expansionUpperAlert();
  if (frontObstacle != sentFront && nav::elapsed(millis(),frontEventAt,300)) {
    sentFront = frontObstacle; frontEventAt = millis();
    sendMessage(frontObstacle ? "OBSTACLE:UPPER" : "OBSTACLE:UPPER_CLEAR");
  }
  feedbackTick(stairAlert,leftCm,rightCm,frontObstacle ? frontCm : -1); brakeSet(stairAlert);
  bleTick();
  delay(2);
}
`````

## 파일: src/profiles.cpp

SHA-256: `f1bb3384323041e35279a476208e7ae570eb5fb9d8e851c3173141422d974d10`

`````cpp
#include "profiles.h"
#include "feedback.h"
#include "ble_link.h"
#include "expansion.h"
#include "config.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <ctype.h>
constexpr int PROFILE_MAX_COUNT = 500, PROFILE_NAME_MAX_LEN = 12, RFID_UID_MAX_LEN = 10;
constexpr uint16_t OBSTACLE_CM_MIN = 20, OBSTACLE_CM_MAX = 200;
constexpr uint8_t HEIGHT_CODE_MIN = 14, HEIGHT_CODE_MAX = 19, VIB_LEVEL_MAX = 1;
constexpr uint8_t DEFAULT_HEIGHT_CODE = cfg::DEFAULT_HEIGHT_CODE, DEFAULT_VIB_LEVEL = cfg::DEFAULT_VIB_LEVEL;
constexpr uint16_t DEFAULT_OBSTACLE_CM = cfg::DEFAULT_OBSTACLE_CM;
constexpr uint32_t VIB_DUTY_WEAK = cfg::VIB_WEAK_DUTY, VIB_DUTY_STRONG = cfg::VIB_STRONG_DUTY;
constexpr uint32_t REG_WAIT_MS = cfg::REG_WAIT_MS, RFID_SAME_CARD_COOLDOWN_MS = 2000;
constexpr char DEFAULT_PROFILE_NAME[] = "DEFAULT";
const auto &STAIR_THRESHOLD_BY_HEIGHT_CM = cfg::STAIR_CM_BY_HEIGHT;
float activeObstacleThresholdCM = cfg::DEFAULT_OBSTACLE_CM;
float activeStairThresholdCM = cfg::DEFAULT_STAIR_CM;
uint32_t activeVibDuty = cfg::VIB_STRONG_DUTY;
char activeProfileName[13] = "DEFAULT";

const char PROFILE_FILE_PATH[] = "/profiles.bin";

// 고정 길이 레코드 28바이트. 파일 내 위치 = 인덱스 x 28
struct __attribute__((packed)) ProfileRecord
{
  uint8_t uid[RFID_UID_MAX_LEN];
  uint8_t uidLen;
  char name[PROFILE_NAME_MAX_LEN + 1];
  uint16_t obstacleCm;
  uint8_t heightCode;
  uint8_t vib;
};
static_assert(sizeof(ProfileRecord) == 28, "ProfileRecord must be 28 bytes");

ProfileRecord profiles[PROFILE_MAX_COUNT];   // 부팅 시 전체 로드 (약 14KB RAM)
int profileCount = 0;
bool profileStoreReady = false;

struct PendingConfig
{
  bool valid;
  uint32_t session;
  char name[PROFILE_NAME_MAX_LEN + 1];
  uint16_t obstacleCm;
  uint8_t heightCode;
  uint8_t vib;
};
PendingConfig pendingCfg = {};
bool regWaiting = false;
unsigned long regStartTime = 0;

uint8_t lastHandledUid[RFID_UID_MAX_LEN] = {};
uint8_t lastHandledUidLen = 0;
unsigned long lastHandledTime = 0;
String uidToHex(const uint8_t *uid, uint8_t len)
{
  static const char HEX_CHARS[] = "0123456789ABCDEF";
  String out;
  for (uint8_t i = 0; i < len; i++)
  {
    out += HEX_CHARS[uid[i] >> 4];
    out += HEX_CHARS[uid[i] & 0x0F];
  }
  return out;
}

float stairThresholdForHeight(uint8_t heightCode)
{
  if (heightCode < HEIGHT_CODE_MIN || heightCode > HEIGHT_CODE_MAX)
    heightCode = DEFAULT_HEIGHT_CODE;
  return STAIR_THRESHOLD_BY_HEIGHT_CM[heightCode - HEIGHT_CODE_MIN];
}

void applyProfileValues(const char *name, uint16_t obstacleCm, uint8_t heightCode, uint8_t vib)
{
  activeObstacleThresholdCM = obstacleCm;
  activeStairThresholdCM = stairThresholdForHeight(heightCode);
  activeVibDuty = vib ? VIB_DUTY_STRONG : VIB_DUTY_WEAK;
  strncpy(activeProfileName, name, PROFILE_NAME_MAX_LEN);
  activeProfileName[PROFILE_NAME_MAX_LEN] = '\0';
  // main resets its stair filter when the threshold changes.
  Serial.println("[RFID] 프로필 적용: " + String(activeProfileName) +
                 " / 장애물 " + String(activeObstacleThresholdCM, 0) +
                 "cm / 계단 " + String(activeStairThresholdCM, 0) +
                 "cm / 진동 " + (vib ? "강" : "약"));
}

void applyDefaultProfile()
{
  applyProfileValues(DEFAULT_PROFILE_NAME, DEFAULT_OBSTACLE_CM,
                     DEFAULT_HEIGHT_CODE, DEFAULT_VIB_LEVEL);
}

int findProfileIndex(const uint8_t *uid, uint8_t len)
{
  for (int i = 0; i < profileCount; i++)
  {
    if (profiles[i].uidLen == len && memcmp(profiles[i].uid, uid, len) == 0) return i;
  }
  return -1;
}

bool isRecordValid(const ProfileRecord &r)
{
  if (r.uidLen != 4 && r.uidLen != 7 && r.uidLen != 10) return false;
  if (r.heightCode < HEIGHT_CODE_MIN || r.heightCode > HEIGHT_CODE_MAX) return false;
  if (r.obstacleCm < OBSTACLE_CM_MIN || r.obstacleCm > OBSTACLE_CM_MAX) return false;
  if (r.vib > VIB_LEVEL_MAX) return false;
  size_t n = strnlen(r.name, sizeof(r.name));
  if (n == 0 || n > PROFILE_NAME_MAX_LEN) return false;
  for (size_t i = 0; i < n; ++i) if (!isalnum((unsigned char)r.name[i])) return false;
  return true;
}

// CRC-protected snapshots. Keep the previous file intact until atomic rename.
struct StoreHeader { uint32_t magic; uint16_t version; uint16_t count; uint32_t crc; };
constexpr uint32_t STORE_MAGIC = 0x4E415643;
constexpr char TEMP_PATH[] = "/profiles.tmp";
uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFF;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int b = 0; b < 8; ++b) c = (c >> 1) ^ ((0u-(c&1u)) & 0xEDB88320u);
  }
  return ~c;
}
bool mountProfiles() {
  if (LittleFS.begin(false)) return true;
  // Format only a genuinely blank factory partition, never a corrupt existing store.
  const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  if (!part) return false;
  uint8_t block[256];
  for (size_t pos = 0; pos < part->size; pos += sizeof(block)) {
    if (esp_partition_read(part, pos, block, sizeof(block)) != ESP_OK) return false;
    for (uint8_t byte : block) if (byte != 0xFF) return false;
  }
  if (!LittleFS.format()) return false;
  return LittleFS.begin(false);
}
void loadProfiles() {
  profileCount = 0; profileStoreReady = mountProfiles();
  if (!profileStoreReady) { Serial.println("[카드 저장소 오류] 파일시스템을 열지 못했습니다. 기존 데이터는 보존하며 카드 등록을 중지합니다."); return; }
  if (!LittleFS.exists(PROFILE_FILE_PATH)) { Serial.println("[카드 저장소] 아직 등록된 카드 파일이 없습니다. 최초 등록 전에는 정상입니다."); return; }
  File f = LittleFS.open(PROFILE_FILE_PATH, FILE_READ);
  if (!f) { profileStoreReady = false; return; }
  StoreHeader h = {}; bool header = f.read((uint8_t *)&h, sizeof(h)) == sizeof(h) && h.magic == STORE_MAGIC;
  size_t size = f.size();
  int count = header ? h.count : size / sizeof(ProfileRecord);
  bool valid = count >= 0 && count <= PROFILE_MAX_COUNT;
  if (header) valid = valid && h.version == 1 && size == sizeof(h)+count*sizeof(ProfileRecord);
  else { valid = valid && size % sizeof(ProfileRecord) == 0; f.seek(0); }
  if (valid && count) valid = f.read((uint8_t *)profiles, count*sizeof(ProfileRecord)) == count*sizeof(ProfileRecord);
  f.close();
  if (valid && header) valid = crc32((uint8_t *)profiles, count*sizeof(ProfileRecord)) == h.crc;
  for (int i = 0; valid && i < count; ++i) valid = isRecordValid(profiles[i]);
  if (!valid) {
    profileStoreReady = false; // do not overwrite a damaged file on next registration
    Serial.println("[카드 저장소 오류] 프로필 파일이 손상됐습니다. 파일은 보존하며 카드 등록을 중지합니다.");
  } else { profileCount = count; Serial.printf("[카드 저장소] %d명 불러오기 완료 (이전 파일 형식=%d)\n", count, !header); }
}
bool saveProfileAt(int index, const ProfileRecord &rec, bool isNew) {
  if (!profileStoreReady) return false;
  const int count = profileCount + (isNew ? 1 : 0);
  // Build the prospective snapshot in RAM, restore it until durable commit succeeds.
  const ProfileRecord previous = profiles[index];
  profiles[index] = rec;
  StoreHeader h = {STORE_MAGIC, 1, uint16_t(count), crc32((uint8_t *)profiles, count*sizeof(ProfileRecord))};
  File f = LittleFS.open(TEMP_PATH, FILE_WRITE);
  bool ok = bool(f);
  if (ok) ok = f.write((uint8_t *)&h, sizeof(h)) == sizeof(h);
  if (ok) ok = f.write((uint8_t *)profiles, count*sizeof(ProfileRecord)) == count*sizeof(ProfileRecord);
  if (f) { f.flush(); f.close(); }
  profiles[index] = previous;
  if (!ok) return false;
  // Verify bytes before replacing the previous snapshot.
  f = LittleFS.open(TEMP_PATH, FILE_READ);
  StoreHeader check = {};
  ok = bool(f) && f.size() == sizeof(h)+count*sizeof(ProfileRecord) &&
       f.read((uint8_t *)&check, sizeof(check)) == sizeof(check) && memcmp(&check, &h, sizeof(h)) == 0;
  uint32_t crc = 0xFFFFFFFF;
  while (ok && f.available()) {
    crc ^= uint8_t(f.read());
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ ((0u-(crc&1u)) & 0xEDB88320u);
  }
  if (f) f.close();
  return ok && ~crc == h.crc && LittleFS.rename(TEMP_PATH, PROFILE_FILE_PATH);
}

// ---------- BLE: CFG / REG ----------

bool parseStrictUInt(const String &s, long &out)
{
  if (s.length() == 0 || s.length() > 5) return false;
  for (unsigned int i = 0; i < s.length(); i++)
    if (s[i] < '0' || s[i] > '9') return false;
  out = s.toInt();
  return true;
}

// 반환: 0=OK, 1=FORMAT, 2=RANGE
int parseCfg(const String &line, PendingConfig &out)
{
  if (!line.startsWith("CFG,")) return 1;
  String fields[4];
  int start = 4;
  for (int i = 0; i < 4; i++)
  {
    int comma = line.indexOf(',', start);
    if (i < 3)
    {
      if (comma < 0) return 1;
      fields[i] = line.substring(start, comma);
      start = comma + 1;
    }
    else
    {
      if (comma >= 0) return 1;  // 필드 초과
      fields[i] = line.substring(start);
    }
  }

  const String &name = fields[0];
  if (name.length() < 1 || name.length() > (unsigned int)PROFILE_NAME_MAX_LEN) return 1;
  for (unsigned int i = 0; i < name.length(); i++)
    if (!isalnum((unsigned char)name[i])) return 1;

  long obstacle, height, vib;
  if (!parseStrictUInt(fields[1], obstacle) ||
      !parseStrictUInt(fields[2], height) ||
      !parseStrictUInt(fields[3], vib)) return 1;

  if (obstacle < OBSTACLE_CM_MIN || obstacle > OBSTACLE_CM_MAX) return 2;
  if (height < HEIGHT_CODE_MIN || height > HEIGHT_CODE_MAX) return 2;
  if (vib < 0 || vib > VIB_LEVEL_MAX) return 2;

  memset(&out, 0, sizeof(out));
  strncpy(out.name, name.c_str(), PROFILE_NAME_MAX_LEN);
  out.obstacleCm = (uint16_t)obstacle;
  out.heightCode = (uint8_t)height;
  out.vib = (uint8_t)vib;
  out.valid = true;
  return 0;
}

void handleCfgCommand(const String &line)
{
  PendingConfig cfg;
  int result = parseCfg(line, cfg);
  if (result == 1) { sendProtocolMessage("ERR,CFG,FORMAT"); return; }
  if (result == 2) { sendProtocolMessage("ERR,CFG,RANGE"); return; }
  pendingCfg = cfg;
  pendingCfg.session = bleSessionEpoch();
  profilesCancelRegistration("CONFIG_CHANGED"); // require a new REG for changed settings
  Serial.printf("[RFID] 대기 설정 수신: %s\n", pendingCfg.name);
  sendProtocolMessage("ACK,CFG");
}

void startRegistration()
{
  if (pendingCfg.valid && (!bleConnected() || pendingCfg.session != bleSessionEpoch())) profilesClearPending();
  if (!pendingCfg.valid) { sendProtocolMessage("REG,NOCFG"); return; }
  if (!profileStoreReady) { sendProtocolMessage("REG,FSERR"); return; }
  regWaiting = true;               // 이미 대기 중이면 15초 재시작
  regStartTime = millis();
  // Existing card may be registered on the next UID poll.
  Serial.println("[RFID] 등록 대기 시작 (15초)");
  sendProtocolMessage("REG,WAIT," + String(REG_WAIT_MS / 1000));
}

void updateRegistrationTimeout()
{
  if (!regWaiting) return;
  if (millis() - regStartTime < REG_WAIT_MS) return;
  regWaiting = false;              // 대기 설정은 유지
  Serial.println("[RFID] 등록 대기 시간 초과");
  sendProtocolMessage("REG,TIMEOUT");
  feedbackStart(Feedback::Failed);
}

void registerCard(const uint8_t *uid, uint8_t len, const String &uidHex)
{
  regWaiting = false;
  int idx = findProfileIndex(uid, len);
  bool isNew = idx < 0;
  if (isNew && profileCount >= PROFILE_MAX_COUNT)
  {
    sendProtocolMessage("REG,FULL");
    feedbackStart(Feedback::Failed);
    return;
  }

  ProfileRecord rec;
  memset(&rec, 0, sizeof(rec));
  memcpy(rec.uid, uid, len);
  rec.uidLen = len;
  strncpy(rec.name, pendingCfg.name, PROFILE_NAME_MAX_LEN);
  rec.obstacleCm = pendingCfg.obstacleCm;
  rec.heightCode = pendingCfg.heightCode;
  rec.vib = pendingCfg.vib;

  int target = isNew ? profileCount : idx;
  if (!saveProfileAt(target, rec, isNew))
  {
    Serial.println("[RFID] 프로필 저장 실패");
    sendProtocolMessage("REG,FSERR");
    feedbackStart(Feedback::Failed);
    return;
  }
  profiles[target] = rec;          // 저장 성공 후에만 RAM 반영
  if (isNew) profileCount++;

  applyProfileValues(rec.name, rec.obstacleCm, rec.heightCode, rec.vib);
  Serial.printf("[RFID] 등록 %s: %s (%s), 총 %d명\n",
                isNew ? "신규" : "덮어쓰기", uidHex.c_str(), rec.name, profileCount);
  sendProtocolMessage("REG,OK," + uidHex + (isNew ? ",NEW," : ",UPDATE,") + String(rec.name));
  feedbackStart(Feedback::Registered);
}

void handleCardDetected(const uint8_t *uid, uint8_t len)
{
  String uidHex = uidToHex(uid, len);
  unsigned long now = millis();

  if (regWaiting)
  {
    registerCard(uid, len, uidHex);
  }
  else
  {
    // 카드가 경계에서 흔들려 떨어짐/재인식이 반복돼도 같은 카드는 쿨다운 동안 무시
    bool sameCard = lastHandledUidLen == len && memcmp(lastHandledUid, uid, len) == 0;
    if (sameCard && now - lastHandledTime < RFID_SAME_CARD_COOLDOWN_MS) return;

    int idx = findProfileIndex(uid, len);
    if (idx >= 0)
    {
      const ProfileRecord &r = profiles[idx];
      applyProfileValues(r.name, r.obstacleCm, r.heightCode, r.vib);
      sendProtocolMessage("APPLY," + uidHex + "," + String(r.name));
      feedbackStart(Feedback::Applied);
    }
    else
    {
      Serial.println("[RFID] 미등록 카드 " + uidHex + " -> 기본 모드");
      applyDefaultProfile();
      sendProtocolMessage("UNREG," + uidHex);
      feedbackStart(Feedback::Unregistered);
    }
  }

  memcpy(lastHandledUid, uid, len);
  lastHandledUidLen = len;
  lastHandledTime = now;
}


void profilesBegin() { applyDefaultProfile(); loadProfiles(); }
void profilesCard(const uint8_t *uid, uint8_t length) {
  // Card polling happens independently of command processing. Validate before writing.
  if (regWaiting && (!bleConnected() || pendingCfg.session != bleSessionEpoch())) {
    profilesClearPending(); return;
  }
  if (regWaiting && uint32_t(millis()-regStartTime) >= REG_WAIT_MS) {
    updateRegistrationTimeout(); return;
  }
  if (!profileStoreReady) { sendProtocolMessage("REG,FSERR"); return; }
  if (length == 4 || length == 7 || length == 10) handleCardDetected(uid, length);
}
void profilesTick() {
  if (pendingCfg.valid && (!bleConnected() || pendingCfg.session != bleSessionEpoch())) profilesClearPending();
  updateRegistrationTimeout();
}
bool profilesStoreReady() { return profileStoreReady; }
void profilesClearPending() { pendingCfg.valid=false; profilesCancelRegistration("DISCONNECTED"); }
void profilesSerialCommand(const String &c) {
  if (c != "FS_INIT_CONFIRM_ERASE_PROFILES") return;
  // Maintenance-only explicit command; never run automatically or expose via BLE.
  if (profileStoreReady) { Serial.println("[카드 저장소] 정상 상태이므로 초기화를 거부합니다."); return; }
  LittleFS.end();
  if (!LittleFS.format()) { Serial.println("[카드 저장소 오류] 초기화에 실패했습니다."); return; }
  loadProfiles();
  Serial.println(profileStoreReady ? "[카드 저장소] 초기화 완료. 카드 등록을 사용할 수 있습니다." : "[카드 저장소 오류] 초기화 후에도 저장소를 사용할 수 없습니다.");
}
bool profilesWaiting() { return regWaiting; }
void profilesCancelRegistration(const char *reason) {
  if (regWaiting) { regWaiting = false; sendProtocolMessage(String("REG,CANCEL,")+reason); }
}
bool profilesCommand(const String &c) {
  if (c == "CFG" || c.startsWith("CFG,")) { handleCfgCommand(c); return true; }
  if (c == "REG") {
    if (!expansionRfidReady()) sendProtocolMessage("REG,NO_RFID");
    else startRegistration();
    return true;
  }
  if (c == "GET_PROFILE_STORE") { sendMessage(String("SENSOR:PROFILE_STORE:")+(profileStoreReady ? "READY" : "FAULT")); return true; }
  if (c == "REG_CANCEL") { profilesCancelRegistration("USER"); return true; }
  if (c.startsWith("SET_STAIR_CM,")) {
    long cm;
    if (!parseStrictUInt(c.substring(13), cm) || cm < 30 || cm > 600) sendMessage("ERR,STAIR,RANGE");
    else { activeStairThresholdCM = cm; sendMessage("ACK,STAIR,"+String(cm)); }
    return true;
  }
  return false;
}
`````

## 파일: src/sensors.cpp

SHA-256: `6a13250432ffcd5f58aa86f20ed2d20ea5404289e886d29fe724a0c0c6a10f65`

`````cpp
#include "sensors.h"
#include "config.h"
#include "logic.h"
#include <Wire.h>
#include "segment_bus.h"
#include <new>
#include <SparkFun_VL53L5CX_Library.h>
#include <vl53l4cx_class.h>

namespace {
SemaphoreHandle_t segmentMutex = nullptr;
class BusGuard {
 public:
  bool ok = navSegmentLock();
  ~BusGuard() { if (ok) navSegmentUnlock(); }
};
TwoWire segment(1);
VL53L4CX stair(&segment, -1); // 4 wires: onboard XSHUT must be held HIGH
SparkFun_VL53L5CX obstacle;
SensorSnapshot shared;
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
nav::FallDetector fall;
const char *mpuReadReason = "연결 확인 중";
const char *stairReadReason = "연결 확인 중";
const char *obstacleInitReason = "초기화 대기";
void publishMpu(const SensorSnapshot &s) {
  portENTER_CRITICAL(&snapshotMux);
  shared.mpuReady = s.mpuReady; shared.mpuConnected = s.mpuConnected;
  shared.mpuReason = s.mpuReason;
  shared.accel = s.accel; shared.accelAt = s.accelAt; shared.accelSeq = s.accelSeq;
  shared.fallCount = s.fallCount; shared.maxAccelGapMs = s.maxAccelGapMs;
  portEXIT_CRITICAL(&snapshotMux);
}
void publishStair(const SensorSnapshot &s) {
  portENTER_CRITICAL(&snapshotMux);
  shared.stairReady = s.stairReady; shared.stairConnected = s.stairConnected;
  shared.stairReason = s.stairReason;
  shared.stairCm = s.stairCm; shared.stairNoTarget = s.stairNoTarget;
  shared.stairAt = s.stairAt; shared.stairSeq = s.stairSeq;
  portEXIT_CRITICAL(&snapshotMux);
}
void publishObstacle(bool ready, float left, float right, uint32_t at, uint32_t seq) {
  portENTER_CRITICAL(&snapshotMux);
  shared.obstacleReady = ready; shared.leftCm = left; shared.rightCm = right;
  shared.obstacleReason = ready ? "정상" : obstacleInitReason;
  shared.obstacleAt = at; shared.obstacleSeq = seq;
  portEXIT_CRITICAL(&snapshotMux);
}
bool present(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr); return bus.endTransmission() == 0;
}
bool stairIdentity() {
  BusGuard guard; if (!guard.ok) { stairReadReason="공유 I2C 사용권 대기 시간 초과"; return false; }
  segment.beginTransmission(0x29);
  segment.write(uint8_t(0x01)); segment.write(uint8_t(0x0F));
  if (segment.endTransmission(false) != 0) { stairReadReason="모델 확인 요청 실패"; return false; }
  if (segment.requestFrom(uint8_t(0x29), uint8_t(2)) != 2) { stairReadReason="미연결 또는 모델 ID 읽기 실패 (주소 0x29)"; return false; }
  const uint8_t high = segment.read();
  const uint8_t low = segment.read();
  const uint16_t id = (uint16_t(high) << 8) | low;
  stairReadReason=id==0xEBAA ? "모델 확인 완료: 연결 안정 확인 중" : "센서 모델 불일치: VL53L4CX(0xEBAA)가 필요함";
  return id == 0xEBAA; // reject L0X/L1X at the same default address
}
// Caller holds the transaction mutex. Recovery never interrupts another transaction.
bool recoverBus() {
  if (digitalRead(cfg::SEGMENT_SDA) && digitalRead(cfg::SEGMENT_SCL)) return false;
  segment.end();
  pinMode(cfg::SEGMENT_SDA, INPUT_PULLUP);
  pinMode(cfg::SEGMENT_SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(cfg::SEGMENT_SCL, HIGH);
  for (int i = 0; i < 9 && digitalRead(cfg::SEGMENT_SDA) == LOW; ++i) {
    digitalWrite(cfg::SEGMENT_SCL, LOW); delayMicroseconds(5);
    digitalWrite(cfg::SEGMENT_SCL, HIGH); delayMicroseconds(5);
  }
  pinMode(cfg::SEGMENT_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(cfg::SEGMENT_SDA, LOW); delayMicroseconds(5);
  digitalWrite(cfg::SEGMENT_SCL, HIGH); delayMicroseconds(5);
  digitalWrite(cfg::SEGMENT_SDA, HIGH); delayMicroseconds(5);
  segment.begin(cfg::SEGMENT_SDA, cfg::SEGMENT_SCL, 100000);
  segment.setTimeOut(cfg::I2C_TIMEOUT_MS);
  return true;
}
bool readMpu(uint8_t reg, uint8_t *data, uint8_t count) {
  BusGuard guard; if (!guard.ok) { mpuReadReason="공유 I2C 사용권 대기 시간 초과"; return false; }
  segment.beginTransmission(0x68); segment.write(reg);
  if (segment.endTransmission(false) != 0) { mpuReadReason="I2C 레지스터 읽기 요청 실패"; return false; }
  if (segment.requestFrom(uint8_t(0x68),count) != count) { mpuReadReason="I2C 응답 부족 또는 읽기 실패 (주소 0x68)"; return false; }
  for (uint8_t i=0; i<count; ++i) data[i]=segment.read();
  return true;
}
bool writeMpu(uint8_t reg, uint8_t value) {
  BusGuard guard; if (!guard.ok) { mpuReadReason="공유 I2C 사용권 대기 시간 초과"; return false; }
  segment.beginTransmission(0x68); segment.write(reg); segment.write(value);
  const bool ok=segment.endTransmission()==0;
  if (!ok) mpuReadReason="MPU 설정 쓰기 실패";
  return ok;
}
bool mpuIdentity() {
  uint8_t id = 0; if (!readMpu(0x75,&id,1)) return false;
  mpuReadReason=id==0x68 ? "모델 확인 완료: 연결 안정 확인 중" : "모델 ID 불일치: MPU6050(0x68)이 필요함";
  return id==0x68;
}
bool initMpu() {
  uint8_t id = 0, range = 0, power = 0;
  if (!readMpu(0x75,&id,1)) return false;
  if (id != 0x68) { mpuReadReason="MPU 모델 ID 불일치"; return false; }
  // MPU6050 register map: awake/PLL, 44Hz DLPF, 200Hz sample, gyro 500dps, accel 8g.
  if (!writeMpu(0x6B,0x01) || !writeMpu(0x6C,0x00) || !writeMpu(0x1A,0x03) ||
      !writeMpu(0x19,0x04) || !writeMpu(0x1B,0x08) || !writeMpu(0x1C,0x10)) return false;
  if (!readMpu(0x1C,&range,1) || !readMpu(0x6B,&power,1)) return false;
  if ((range & 0x18) != 0x10 || (power & 0xC0)) { mpuReadReason="설정 검증 실패: 가속도 범위 또는 전원 상태 이상"; return false; }
  vTaskDelay(pdMS_TO_TICKS(50));
  fall.reset(); Serial.println("[가속도 MPU6050] 초기화 완료. SDA=21, SCL=22, 주소=0x68. 새 측정값부터 낙상 판정에 사용합니다."); return true;
}
bool initStair() {
  if (!stairIdentity()) return false;
  // Reinitialize a still-powered sensor after a short contact interruption.
  stair.VL53L4CX_StopMeasurement();
  // Rebuild driver state after a brownout; the bus object remains unchanged.
  stair.~VL53L4CX(); new (&stair) VL53L4CX(&segment, -1);
  int status = stair.InitSensor(0x52); // ST API takes 8-bit address; Wire uses 0x29
  if (!status) status = stair.VL53L4CX_SetDistanceMode(VL53L4CX_DISTANCEMODE_LONG);
  if (!status) status = stair.VL53L4CX_SetMeasurementTimingBudgetMicroSeconds(50000);
  if (!status) status = stair.VL53L4CX_StartMeasurement();
  Serial.printf("[계단 VL53L4CX] 초기화 %s (드라이버 결과=%d, SDA=21/SCL=22, 주소=0x29)\n", status==0 ? "완료" : "실패", status);
  stairReadReason=status==0 ? "첫 측정 대기" : "센서 초기화 실패: 배선·전원·XSHUT HIGH 확인";
  return status == 0;
}
void mpuTask(void *) {
  SensorSnapshot s;
  nav::ReconnectGate reconnect;
  uint32_t probeAt = millis()-cfg::POGO_PROBE_MS, retryAt = millis()-cfg::SENSOR_RETRY_MS;
  uint32_t healthAt = 0, lastFall = millis()-10000;
  uint8_t miss = 0, healthMiss = 0;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    uint32_t now = millis();
    if (!s.mpuReady && nav::elapsed(now,probeAt,cfg::POGO_PROBE_MS)) {
      probeAt = now;
      { BusGuard guard; if (guard.ok) recoverBus(); }
      s.mpuConnected = mpuIdentity(); s.mpuReason=mpuReadReason;
      if (reconnect.update(s.mpuConnected,now) && nav::elapsed(now,retryAt,cfg::SENSOR_RETRY_MS)) {
        Serial.println("[가속도 MPU6050] 연결 안정 확인 완료. 초기화·자동 복구를 시작합니다.");
        s.mpuReady = initMpu(); retryAt = millis(); miss = healthMiss = 0;
        s.mpuReason=s.mpuReady ? "첫 측정 대기" : mpuReadReason;
        if (!s.mpuReady) Serial.println(String("[가속도 MPU6050] 초기화 실패: ")+s.mpuReason+". 안정 확인 후 재시도합니다.");
        if (!s.mpuReady) reconnect.reset();
      }
    }
    if (s.mpuReady) {
      uint8_t raw[6] = {};
      if (readMpu(0x3B,raw,sizeof(raw))) {
        now = millis();
        uint32_t gap = now-s.accelAt;
        if (s.accelSeq && gap > s.maxAccelGapMs) s.maxAccelGapMs = gap;
        if (s.accelSeq && gap > cfg::ACCEL_STALE_MS) fall.reset();
        s.accel = nav::accelMagnitude(raw); s.accelAt = now; ++s.accelSeq; miss = 0; s.mpuReason="정상";
        if (fall.sample(s.accel,now) && nav::elapsed(now,lastFall,10000)) {
          lastFall = now; ++s.fallCount;
        }
      } else if (++miss >= 2) {
        s.mpuReady = s.mpuConnected = false; s.mpuReason=mpuReadReason; fall.reset(); reconnect.reset();
        Serial.println(String("[가속도 MPU6050] 측정 읽기 연속 2회 실패: ")+s.mpuReason+". 낙상 판정을 중지하고 자동 복구합니다.");
        retryAt = now-cfg::SENSOR_RETRY_MS;
      }
      if (s.mpuReady && nav::elapsed(now,healthAt,cfg::SENSOR_HEALTH_MS)) {
        healthAt = now; uint8_t range=0, power=0;
        const bool readOk=readMpu(0x1C,&range,1) && readMpu(0x6B,&power,1);
        const bool invalidConfig=readOk && ((range&0x18)!=0x10 || (power&0xC0));
        if (readOk) healthMiss=0;
        else if (healthMiss<cfg::SENSOR_HEALTH_MISS_LIMIT) ++healthMiss;
        // A known bad scale invalidates immediately; one isolated NACK does not.
        if (invalidConfig || healthMiss>=cfg::SENSOR_HEALTH_MISS_LIMIT) {
          s.mpuReady = false;
          s.mpuReason=invalidConfig ? "가속도 범위 또는 전원 설정이 바뀜" : "설정 상태 읽기 연속 실패";
          Serial.println(String("[가속도 MPU6050] 상태 점검 이상: ")+s.mpuReason+". 재초기화합니다.");
          fall.reset(); reconnect.reset(); retryAt = now-cfg::SENSOR_RETRY_MS;
        }
      }
    }
    publishMpu(s);
    // Absolute schedule, independent of ToF init. Physical bus faults can still delay a read.
    if (xTaskGetTickCount()-wake > pdMS_TO_TICKS(cfg::MPU_PERIOD_MS)) wake=xTaskGetTickCount();
    vTaskDelayUntil(&wake,pdMS_TO_TICKS(cfg::MPU_PERIOD_MS));
  }
}
void stairTask(void *) {
  SensorSnapshot s;
  nav::ReconnectGate reconnect; nav::Mean3 mean;
  uint32_t probeAt=millis()-cfg::POGO_PROBE_MS, retryAt=millis()-cfg::SENSOR_RETRY_MS;
  uint32_t healthAt=0, pollAt=0, watchAt=0;
  uint8_t misses=0, healthMiss=0; bool wasReady=false;
  for (;;) {
    uint32_t now=millis();
    if (!s.stairReady && nav::elapsed(now,probeAt,cfg::POGO_PROBE_MS)) {
      probeAt=now; s.stairConnected=stairIdentity(); s.stairReason=stairReadReason;
      if (reconnect.update(s.stairConnected,now) && nav::elapsed(now,retryAt,cfg::SENSOR_RETRY_MS)) {
        Serial.println("[계단 VL53L4CX] 연결 안정 확인 완료. 초기화·자동 복구를 시작합니다.");
        s.stairReady=initStair(); s.stairReason=stairReadReason; retryAt=millis(); now=retryAt; misses=healthMiss=0; mean.reset();
        s.stairAt=0; s.stairCm=-1; s.stairNoTarget=false; watchAt=millis();
        if (!s.stairReady) reconnect.reset();
      }
    }
    if (s.stairReady && nav::elapsed(now,healthAt,cfg::SENSOR_HEALTH_MS)) {
      healthAt=now;
      if (stairIdentity()) healthMiss=0;
      else if (++healthMiss>=cfg::SENSOR_HEALTH_MISS_LIMIT) { s.stairReady=s.stairConnected=false; s.stairReason=stairReadReason; }
    }
    if (s.stairReady && nav::elapsed(now,pollAt,15)) {
      pollAt=now; uint8_t ready=0;
      int status=stair.VL53L4CX_GetMeasurementDataReady(&ready);
      if (!status && ready) {
        VL53L4CX_MultiRangingData_t data={};
        status=stair.VL53L4CX_GetMultiRangingData(&data);
        if (!status) {
          float nearest=-1;
          for (uint8_t i=0;i<data.NumberOfObjectsFound && i<4;++i) {
            const auto &r=data.RangeData[i];
            if (r.RangeStatus==VL53L4CX_RANGESTATUS_RANGE_VALID && r.RangeMilliMeter>0) {
              float cm=r.RangeMilliMeter/10.0f;
              if (nearest<0 || cm<nearest) nearest=cm;
            }
          }
          s.stairCm=mean.sample(nearest); s.stairNoTarget=data.NumberOfObjectsFound==0;
          s.stairAt=millis(); watchAt=s.stairAt; ++s.stairSeq; misses=0; s.stairReason="정상";
          status=stair.VL53L4CX_ClearInterruptAndStartMeasurement();
        }
      }
      if (status && ++misses>=2) { s.stairReady=false; s.stairReason="측정 통신 연속 실패"; }
      if (s.stairReady && nav::elapsed(millis(),watchAt,cfg::STAIR_STALE_MS)) { s.stairReady=false; s.stairReason="600ms 이상 측정 프레임 없음"; }
    }
    if (!s.stairReady) {
      s.stairCm=-1; mean.reset();
      // Reset qualification after loss, but do not reset it every disconnected loop.
      if (wasReady) { reconnect.reset(); retryAt=millis()-cfg::SENSOR_RETRY_MS; }
    }
    wasReady=s.stairReady;
    publishStair(s);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
bool initObstacle() {
  obstacleInitReason="초기화 중: LPn 리셋·I2C 버스 복구";
  publishObstacle(false,-1,-1,millis(),0);
  Serial.println("[장애물 VL53L5CX] 초기화·자동 복구 시작. SDA=18/SCL=19, LPn=25. 센서 펌웨어 전송에는 시간이 걸립니다.");
  // LPn disables the interface; it is not a guaranteed full power reset.
  digitalWrite(cfg::OBSTACLE_LPN, LOW);
  Wire.end();
  // This bus has one owner. Release a slave waiting for clocks, then issue STOP.
  pinMode(cfg::OBSTACLE_SDA, INPUT_PULLUP);
  pinMode(cfg::OBSTACLE_SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(cfg::OBSTACLE_SCL,HIGH);
  for (int i=0;i<9 && digitalRead(cfg::OBSTACLE_SDA)==LOW;++i) {
    digitalWrite(cfg::OBSTACLE_SCL,LOW); delayMicroseconds(5);
    digitalWrite(cfg::OBSTACLE_SCL,HIGH); delayMicroseconds(5);
  }
  pinMode(cfg::OBSTACLE_SDA,OUTPUT_OPEN_DRAIN);
  digitalWrite(cfg::OBSTACLE_SDA,LOW); delayMicroseconds(5);
  digitalWrite(cfg::OBSTACLE_SCL,HIGH); delayMicroseconds(5);
  digitalWrite(cfg::OBSTACLE_SDA,HIGH);
  vTaskDelay(pdMS_TO_TICKS(cfg::OBSTACLE_LPN_LOW_MS));
  Wire.begin(cfg::OBSTACLE_SDA,cfg::OBSTACLE_SCL,100000);
  Wire.setTimeOut(cfg::OBSTACLE_I2C_TIMEOUT_MS);
  digitalWrite(cfg::OBSTACLE_LPN, HIGH); vTaskDelay(pdMS_TO_TICKS(cfg::OBSTACLE_BOOT_MS));
  if (!present(Wire, 0x29)) { obstacleInitReason="주소 0x29 응답 없음: 18/19·LPn25·전원 확인"; return false; }
  obstacleInitReason="초기화 중: 센서 펌웨어 전송"; publishObstacle(false,-1,-1,millis(),0);
  Serial.println("[장애물 VL53L5CX] 주소 응답 확인. 센서 펌웨어 전송을 시작합니다.");
  if (!obstacle.begin(0x29, Wire)) { obstacleInitReason="센서 펌웨어 전송 또는 초기화 실패"; return false; }
  Wire.setClock(400000);
  if (!obstacle.setResolution(64)) { obstacleInitReason="64구역 측정 설정 실패"; return false; }
  if (!obstacle.setRangingFrequency(15)) { obstacleInitReason="측정 주파수 설정 실패"; return false; }
  if (!obstacle.startRanging()) { obstacleInitReason="거리 측정 시작 실패"; return false; }
  obstacleInitReason="첫 측정 프레임 대기"; return true;
}
void obstacleTask(void *) {
  Wire.begin(cfg::OBSTACLE_SDA, cfg::OBSTACLE_SCL, 100000);
  Wire.setTimeOut(cfg::OBSTACLE_I2C_TIMEOUT_MS);
  nav::ObstacleRecovery recovery; nav::Mean3 leftMean, rightMean;
  uint32_t at = 0, seq = 0;
  for (;;) {
    uint32_t now = millis();
    if (recovery.shouldInitialize(now)) {
      bool ready = initObstacle(); at = millis(); recovery.initialized(ready,at);
      leftMean.reset(); rightMean.reset();
      if (ready) Serial.println("[장애물 VL53L5CX] 초기화 완료. 첫 거리 측정을 기다립니다.");
      else Serial.println(String("[장애물 VL53L5CX] 초기화 실패: ")+obstacleInitReason+". 실패 종료 후 5초 뒤 재시도합니다.");
      publishObstacle(false, -1, -1, at, seq);
    }
    if (recovery.running() && obstacle.isDataReady()) {
      VL53L5CX_ResultsData data;
      if (obstacle.getRangingData(&data)) {
        float left = -1, right = -1;
        for (int i = 0; i < 64; ++i) {
          const int row = 7 - i/8;
          if (row < cfg::OBSTACLE_ROW_MIN || row > cfg::OBSTACLE_ROW_MAX ||
              data.target_status[i] != 5 || data.distance_mm[i] <= 0) continue;
          float cm = data.distance_mm[i] / 10.0f;
          float &side = i%8 < 4 ? left : right;
          if (side < 0 || cm < side) side = cm;
        }
        if (cfg::SWAP_OBSTACLE_SIDES) { float temp = left; left = right; right = temp; }
        at = millis(); recovery.frame(at); ++seq;
        publishObstacle(true, leftMean.sample(left), rightMean.sample(right), at, seq);
      }
    }
    if (recovery.needsRestart(millis())) {
      // Main already ignores frames aged >=600ms. At >=2.5s restart without backoff.
      obstacleInitReason="2.5초 이상 측정 프레임 없음: 즉시 재초기화 예약";
      Serial.println("[장애물 VL53L5CX] 2.5초 이상 측정 프레임이 없습니다. 즉시 재초기화합니다.");
      publishObstacle(false, -1, -1, at, seq);
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
}
SensorSnapshot sensorSnapshot() {
  portENTER_CRITICAL(&snapshotMux); SensorSnapshot result = shared;
  portEXIT_CRITICAL(&snapshotMux); return result;
}
extern "C" bool navSegmentLock() {
  return segmentMutex && xSemaphoreTake(segmentMutex,pdMS_TO_TICKS(60))==pdTRUE;
}
extern "C" void navSegmentUnlock() { xSemaphoreGive(segmentMutex); }
bool sensorsBegin() {
  segmentMutex=xSemaphoreCreateMutex(); if (!segmentMutex) return false;
  segment.begin(cfg::SEGMENT_SDA,cfg::SEGMENT_SCL,100000);
  segment.setTimeOut(cfg::I2C_TIMEOUT_MS);
  pinMode(cfg::OBSTACLE_LPN,OUTPUT); digitalWrite(cfg::OBSTACLE_LPN,LOW);
  TaskHandle_t mpuHandle=nullptr, stairHandle=nullptr, obstacleHandle=nullptr;
  const auto a=xTaskCreatePinnedToCore(mpuTask,"mpu",4096,nullptr,3,&mpuHandle,0);
  const auto b=xTaskCreatePinnedToCore(stairTask,"stair",12288,nullptr,2,&stairHandle,0);
  const auto c=xTaskCreatePinnedToCore(obstacleTask,"obstacle",8192,nullptr,1,&obstacleHandle,0);
  return a==pdPASS && b==pdPASS && c==pdPASS;
}
`````

## 파일: android_patch/BleManager.kt

SHA-256: `ab5170f1bb377ff103440683721ebd78ce694a182b79e03bcc8358272f40da18`

`````kotlin
package com.example.whitecaneguardian

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import java.io.ByteArrayOutputStream
import java.util.UUID
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CopyOnWriteArraySet

/** NAVCANE_Final protocol. Existing public listener/sendCommand API is retained. */
class BleManager(private val context: Context) {
    companion object {
        const val DEVICE_NAME = "NAVCANE_ESP32"
        val SERVICE_UUID: UUID = UUID.fromString("12345678-1234-1234-1234-1234567890ab")
        val CHARACTERISTIC_UUID: UUID = UUID.fromString("abcd1234-1234-1234-1234-abcdef123456")
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val REQUEST_MTU = 185
        private const val MAX_LINE_BYTES = 96
        private const val MAX_QUEUE = 32
    }
    interface Listener {
        fun onStatusChanged(connected: Boolean)
        fun onMessageReceived(message: String)
        fun onRfidResponse(line: String) {}
    }
    // Primary listener remains available to the app's fall/emergency handler.
    @Volatile var listener: Listener? = null
    private val additionalListeners = CopyOnWriteArraySet<Listener>()
    fun addListener(value: Listener) { additionalListeners.add(value) }
    fun removeListener(value: Listener) { additionalListeners.remove(value) }
    private fun listeners(): Set<Listener> = additionalListeners.toSet() + listOfNotNull(listener)

    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private var scanner: BluetoothLeScanner? = null
    @Volatile private var gatt: BluetoothGatt? = null
    private var scanning = false
    private var scanGeneration = 0
    @Volatile private var connected = false
    @Volatile private var writeCharacteristic: BluetoothGattCharacteristic? = null
    private val handler = Handler(Looper.getMainLooper())
    @Volatile private var writeReady = false
    private var writeInProgress = false
    private var servicesRequested = false
    private var negotiatedMtu = 23
    private val writeQueue = ConcurrentLinkedQueue<ByteArray>()
    private var currentWrite: ByteArray? = null
    private var writeOffset = 0
    private var lastChunkSize = 0
    private var writeToken = 0
    private var writeRetries = 0
    private var autoReconnect = true
    private val lines = NavcaneLines(MAX_LINE_BYTES)

    private fun log(type: String, detail: String) {
        BleLog.add(BleLogEntry(System.currentTimeMillis(), type, detail))
    }
    @SuppressLint("MissingPermission")
    fun startScan() {
        if (scanning || connected || gatt != null || bluetoothAdapter?.isEnabled != true) return
        autoReconnect = true
        scanner = bluetoothAdapter.bluetoothLeScanner ?: return
        scanning = true
        scanner?.startScan(scanCallback)
        val token = ++scanGeneration
        handler.postDelayed({
            if (scanning && token == scanGeneration) {
                stopScan()
                if (autoReconnect) handler.postDelayed({ if (autoReconnect && gatt == null) startScan() }, 2000)
            }
        }, 10000)
    }
    @SuppressLint("MissingPermission")
    fun stopScan() {
        if (!scanning) return
        scanning = false; ++scanGeneration
        scanner?.stopScan(scanCallback)
    }
    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (result.device.name != DEVICE_NAME) return
            handler.post {
                if (!scanning || gatt != null) return@post
                log("연결", "기기 발견: $DEVICE_NAME")
                stopScan()
                gatt = result.device.connectGatt(context, false, gattCallback)
                val pending = gatt
                handler.postDelayed({
                    if (gatt === pending && !connected && pending != null) failConnection(pending, "연결 시간 초과")
                }, 10000)
            }
        }
        override fun onScanFailed(errorCode: Int) {
            handler.post { scanning = false; log("오류", "BLE 검색 실패: $errorCode") }
        }
    }
    @SuppressLint("MissingPermission")
    private fun discover(g: BluetoothGatt) {
        if (gatt !== g || servicesRequested) return
        servicesRequested = true
        if (!g.discoverServices()) { failConnection(g, "서비스 검색 요청 실패"); return }
        handler.postDelayed({
            if (gatt === g && !writeReady) failConnection(g, "BLE 준비 시간 초과")
        }, 10000)
    }
    @SuppressLint("MissingPermission")
    private val gattCallback: BluetoothGattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            handler.post {
                if (gatt !== g) { g.close(); return@post }
                if (status != BluetoothGatt.GATT_SUCCESS || newState == BluetoothProfile.STATE_DISCONNECTED) {
                    failConnection(g, "연결 끊김 (status=$status)")
                } else if (newState == BluetoothProfile.STATE_CONNECTED) {
                    connected = true
                    log("연결", "기기 연결됨")
                    // Ready notification is published only after a successful CCCD write.
                    if (!g.requestMtu(REQUEST_MTU)) discover(g)
                    else handler.postDelayed({ discover(g) }, 3000)
                }
            }
        }
        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            handler.post {
                if (gatt !== g) return@post
                negotiatedMtu = if (status == BluetoothGatt.GATT_SUCCESS) mtu.coerceAtLeast(23) else 23
                log("연결", "MTU: $negotiatedMtu (status=$status)")
                discover(g)
            }
        }
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            handler.post {
                if (gatt !== g) return@post
                if (status != BluetoothGatt.GATT_SUCCESS) { failConnection(g, "서비스 검색 실패"); return@post }
                val ch = g.getService(SERVICE_UUID)?.getCharacteristic(CHARACTERISTIC_UUID)
                if (ch == null) { failConnection(g, "NAVCANE 특성 없음"); return@post }
                writeCharacteristic = ch
                val descriptor = ch.getDescriptor(CCCD_UUID)
                if (descriptor == null || !g.setCharacteristicNotification(ch, true)) {
                    failConnection(g, "알림 설정 실패"); return@post
                }
                @Suppress("DEPRECATION")
                val started = if (Build.VERSION.SDK_INT >= 33) {
                    g.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
                } else {
                    descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    g.writeDescriptor(descriptor)
                }
                if (!started) failConnection(g, "알림 요청 실패")
            }
        }
        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            handler.post {
                if (gatt !== g || descriptor.uuid != CCCD_UUID || writeReady) return@post
                if (status != BluetoothGatt.GATT_SUCCESS) { failConnection(g, "알림 준비 실패"); return@post }
                writeReady = true
                log("연결", "알림 준비 완료")
                // Opt into newline framing. Old app remains supported by firmware's default mode.
                currentWrite = "STREAM_ON\n".toByteArray(Charsets.UTF_8)
                writeOffset = 0
                sendCommand("GET_STATUS")
                listeners().forEach { it.onStatusChanged(true) }
                processWriteQueue()
            }
        }
        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            handler.post {
                if (gatt !== g || characteristic.uuid != CHARACTERISTIC_UUID || !writeInProgress) return@post
                writeInProgress = false
                ++writeToken
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    // A failed chunk may have reached ESP32. Do not retry a whole CFG/REG blindly.
                    failConnection(g, "명령 전송 실패 (status=$status)"); return@post
                }
                writeOffset += lastChunkSize
                writeRetries = 0
                if (writeOffset >= (currentWrite?.size ?: 0)) { currentWrite = null; writeOffset = 0 }
                processWriteQueue()
            }
        }
        @Suppress("DEPRECATION")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            // API 33+ uses the value overload; avoid delivering the same notification twice.
            if (Build.VERSION.SDK_INT < 33) receive(g, characteristic.uuid, characteristic.value?.copyOf() ?: byteArrayOf())
        }
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            receive(g, characteristic.uuid, value.copyOf())
        }
    }
    private fun receive(g: BluetoothGatt, uuid: UUID, bytes: ByteArray) {
        handler.post {
            if (gatt !== g || uuid != CHARACTERISTIC_UUID) return@post
            val previousDrops = lines.droppedLines
            lines.push(bytes).forEach { dispatch(it) }
            if (lines.droppedLines != previousDrops) log("오류", "BLE 메시지 길이/형식 오류")
        }
    }
    private fun dispatch(message: String) {
        logParsedEvent(message)
        listeners().forEach { value ->
            value.onMessageReceived(message)
            if (isRfidResponse(message)) value.onRfidResponse(message)
        }
    }
    private fun logParsedEvent(message: String) {
        val (type, detail) = when {
            message == "FALL_DETECTED" -> "낙상" to "낙상 사고 감지"
            message.startsWith("OBSTACLE:") -> "장애물" to when (message.substringAfter(":")) {
                "NONE" -> "좌우 장애물 사라짐"
                "LEFT" -> "좌측 장애물"
                "RIGHT" -> "우측 장애물"
                "BOTH" -> "양측 장애물 (좌우)"
                "UPPER", "FRONT" -> "상부 장애물"
                "UPPER_CLEAR", "FRONT_CLEAR" -> "상부 장애물 사라짐"
                else -> message
            }
            message.startsWith("STAIR_DETECTED") -> "계단" to "계단(내리막) 감지"
            message.startsWith("계단 감지") -> "계단" to "계단(내리막) 감지"
            message == "STAIR_CLEARED" -> "계단" to "계단 해제"
            message.startsWith("포고핀 분리") -> "기기" to "포고핀 분리됨"
            message.startsWith("포고핀 재결합") -> "기기" to "포고핀 재결합됨"
            message == "BUTTON_PRESSED" -> { VoiceTrigger.fire(); "버튼" to "버튼 눌림" }
            message == "GREEN_LIGHT" -> "신호등" to "초록불 감지"
            message == "CONNECTED" -> "연결" to "기기 준비됨 (CONNECTED)"
            message.startsWith("LED:") -> "상태" to "LED ${message.substringAfter(":")}"
            message == "PONG" -> "상태" to "PONG"
            message.startsWith("SENSOR:") -> "센서" to message
            message.startsWith("ACCEL:") -> "센서" to "가속도 ${message.substringAfter(":")}"
            message.startsWith("DISTANCE:") -> "센서" to "전방거리 ${message.substringAfter(":")}cm"
            message.startsWith("STAIR_DISTANCE:") -> "센서" to "계단거리 ${message.substringAfter(":")}cm"
            isRfidResponse(message) -> "RFID" to message
            message.startsWith("ERROR:") -> "오류" to message
            else -> "메시지" to message
        }
        log(type, detail)
    }
    private fun isRfidResponse(msg: String): Boolean =
        msg.startsWith("ACK,") || msg.startsWith("ERR,") || msg.startsWith("REG,") ||
        msg.startsWith("APPLY,") || msg.startsWith("UNREG,")

    fun isConnected(): Boolean = connected
    fun isReady(): Boolean = connected && writeReady
    // Return value means accepted into the local queue, not acknowledged by ESP32.
    fun sendCommand(command: String): Boolean {
        if (!connected || gatt == null || writeCharacteristic == null || writeQueue.size >= MAX_QUEUE) return false
        val framed = if (command.endsWith("\n")) command else "$command\n"
        if (framed.toByteArray(Charsets.UTF_8).size > MAX_LINE_BYTES + 1 || framed.indexOf('\u0000') >= 0) return false
        writeQueue.add(framed.toByteArray(Charsets.UTF_8))
        handler.post { processWriteQueue() }
        return true
    }
    @SuppressLint("MissingPermission")
    private fun processWriteQueue() {
        if (!writeReady || writeInProgress) return
        val g = gatt ?: return
        val ch = writeCharacteristic ?: return
        if (currentWrite == null) { currentWrite = writeQueue.poll() ?: return; writeOffset = 0 }
        val data = currentWrite ?: return
        val end = (writeOffset + (negotiatedMtu - 3).coerceAtLeast(20)).coerceAtMost(data.size)
        val chunk = data.copyOfRange(writeOffset, end)
        lastChunkSize = chunk.size
        writeInProgress = true
        @Suppress("DEPRECATION")
        val started = if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(ch, chunk, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
        } else {
            ch.value = chunk; ch.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            g.writeCharacteristic(ch)
        }
        if (!started) {
            writeInProgress = false
            if (++writeRetries > 3) { failConnection(g, "명령 전송 요청 실패"); return }
            handler.postDelayed({ if (gatt === g) processWriteQueue() }, 150)
            return
        }
        val token = ++writeToken
        handler.postDelayed({
            if (gatt === g && writeInProgress && writeToken == token) failConnection(g, "명령 전송 시간 초과")
        }, 5000)
    }
    @SuppressLint("MissingPermission")
    private fun failConnection(g: BluetoothGatt, reason: String) {
        if (gatt !== g) return
        gatt = null; connected = false; writeReady = false; writeInProgress = false
        servicesRequested = false; negotiatedMtu = 23; writeCharacteristic = null
        writeQueue.clear(); currentWrite = null; writeOffset = 0; writeRetries = 0; ++writeToken
        lines.clear()
        g.disconnect(); g.close()
        log("해제", reason)
        listeners().forEach { it.onStatusChanged(false) }
        if (autoReconnect) handler.postDelayed({ if (autoReconnect && gatt == null) startScan() }, 2000)
    }
    @SuppressLint("MissingPermission")
    fun disconnect() {
        autoReconnect = false
        stopScan()
        gatt?.let { failConnection(it, "사용자가 연결 해제") }
    }
}

/** Byte framing is independent of Android so UTF-8 split boundaries are testable on JVM. */
internal class NavcaneLines(private val maxBytes: Int = 96) {
    private val bytes = ByteArrayOutputStream()
    private var discarding = false
    var droppedLines: Int = 0
        private set
    fun clear() { bytes.reset(); discarding = false; droppedLines = 0 }
    fun push(chunk: ByteArray): List<String> {
        val result = mutableListOf<String>()
        for (byte in chunk) {
            when (byte.toInt() and 0xff) {
                13 -> Unit
                10 -> {
                    if (!discarding) {
                        val line = bytes.toByteArray().toString(Charsets.UTF_8).trim()
                        if (line.isNotEmpty()) result.add(line)
                    }
                    bytes.reset(); discarding = false
                }
                else -> if (!discarding) {
                    if (bytes.size() >= maxBytes || byte.toInt() == 0) {
                        bytes.reset(); discarding = true; ++droppedLines
                    } else bytes.write(byte.toInt())
                }
            }
        }
        return result
    }
}
`````

## 파일: android_patch/RfidConfig.kt

SHA-256: `15b28e85acf62010952181a9107fd80ccd44a6807496e28edf2acb4e0c7034f2`

`````kotlin
package com.example.whitecaneguardian

// RFID 사용자 프로필 연동 상수 (ESP32 팀 확정값 반영)
object RfidConfig {
    const val REQUEST_MTU = 185

    // 이름: 영문/숫자 1~12자, 쉼표·공백 불가
    const val NAME_MAX_LEN = 12
    val NAME_REGEX = Regex("^[A-Za-z0-9]{1,$NAME_MAX_LEN}$")

    // 장애물 감지 시작 거리 (cm) — 확정: 20~200, 기본 50
    const val OBSTACLE_MIN_CM = 20
    const val OBSTACLE_MAX_CM = 200
    const val OBSTACLE_DEFAULT_CM = 50

    // 키 코드 (height_code = 키/10) — 확정: 14~19, 기본 17
    const val HEIGHT_CODE_MIN = 14
    const val HEIGHT_CODE_MAX = 19
    const val HEIGHT_CODE_DEFAULT = 17

    // 진동 세기 — 확정: 0(약)/1(강), 기본 1
    const val VIB_STRONG = 1
    const val VIB_WEAK = 0

    // 카드 등록 대기 시간 (초)
    const val REG_WAIT_SEC = 15

    // 응답 타임아웃 (ms)
    const val ACK_TIMEOUT_MS = 5000L
}
`````

## 파일: android_patch/RfidFragment.kt

SHA-256: `f6e7d4f50e628418a303a18e3607b467082d07c167a0c22b7b88bba90f3a3218`

`````kotlin
package com.example.whitecaneguardian

import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.RadioGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.firestore.FirebaseFirestore

class RfidFragment : Fragment(), BleManager.Listener {

    private lateinit var etName: EditText
    private lateinit var etObstacle: EditText
    private lateinit var tvHeight: TextView
    private lateinit var rgVib: RadioGroup
    private lateinit var btnSend: Button
    private lateinit var btnRegister: Button
    private lateinit var tvStatus: TextView

    private val uiHandler = Handler(Looper.getMainLooper())
    private var requestToken = 0
    private var cfgAcknowledged = false
    private var registering = false
    private var awaitingConfig = false
    private var viewAlive = false
    private var heightCode: Int? = null
    private lateinit var bleManager: BleManager

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View = inflater.inflate(R.layout.fragment_rfid, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        etName = view.findViewById(R.id.etRfidName)
        etObstacle = view.findViewById(R.id.etRfidObstacle)
        tvHeight = view.findViewById(R.id.tvRfidHeight)
        rgVib = view.findViewById(R.id.rgVibration)
        btnSend = view.findViewById(R.id.btnRfidSend)
        btnRegister = view.findViewById(R.id.btnRfidRegister)
        tvStatus = view.findViewById(R.id.tvRfidStatus)

        viewAlive = true
        btnRegister.isEnabled = false
        loadHeightFromProfile()

        btnSend.setOnClickListener { onSendConfig() }
        btnRegister.setOnClickListener { onRegister() }

        bleManager = BleHolder.get(requireContext())
        bleManager.addListener(this)
        onStatusChanged(bleManager.isReady())
    }

    private fun loadHeightFromProfile() {
        val uid = FirebaseAuth.getInstance().currentUser?.uid
        if (uid == null) {
            tvHeight.text = "로그인이 필요합니다"
            return
        }
        FirebaseFirestore.getInstance().collection("users").document(uid)
            .get()
            .addOnSuccessListener { doc ->
                if (!isAdded || !viewAlive) return@addOnSuccessListener
                val height = doc.getDouble("height")
                if (height == null) {
                    tvHeight.text = "신체 정보에 키가 없습니다 (먼저 등록해주세요)"
                    heightCode = null
                } else {
                    val code = (height / 10).toInt()
                    heightCode = code
                    tvHeight.text = "${height.toInt()}cm (코드 $code)"
                }
            }
            .addOnFailureListener {
                if (isAdded && viewAlive) tvHeight.text = "키를 불러오지 못했습니다"
            }
    }

    private fun onSendConfig() {
        val name = etName.text.toString().trim()
        val obstacle = etObstacle.text.toString().toIntOrNull()
        val code = heightCode

        if (!RfidConfig.NAME_REGEX.matches(name)) {
            tvStatus.text = "이름은 영문/숫자 1~12자여야 합니다 (공백·쉼표 불가)"
            return
        }
        if (obstacle == null || obstacle < RfidConfig.OBSTACLE_MIN_CM || obstacle > RfidConfig.OBSTACLE_MAX_CM) {
            tvStatus.text = "장애물 거리는 ${RfidConfig.OBSTACLE_MIN_CM}~${RfidConfig.OBSTACLE_MAX_CM}cm 사이여야 합니다"
            return
        }
        if (code == null || code < RfidConfig.HEIGHT_CODE_MIN || code > RfidConfig.HEIGHT_CODE_MAX) {
            tvStatus.text = "키는 ${RfidConfig.HEIGHT_CODE_MIN * 10}~${RfidConfig.HEIGHT_CODE_MAX * 10 + 9}cm 범위여야 합니다. 신체 정보를 확인해주세요."
            return
        }
        val vib = if (rgVib.checkedRadioButtonId == R.id.rbVibStrong)
            RfidConfig.VIB_STRONG else RfidConfig.VIB_WEAK

        val cfg = "CFG,$name,$obstacle,$code,$vib\n"

        if (!bleManager.isReady()) {
            tvStatus.text = "지팡이가 연결되어 있지 않습니다"
            return
        }
        val ok = bleManager.sendCommand(cfg)
        if (ok) {
            tvStatus.text = "설정 전송 중... (응답 대기)"
            cfgAcknowledged = false
            awaitingConfig = true
            btnSend.isEnabled = false
            btnRegister.isEnabled = false
            armTimeout(RfidConfig.ACK_TIMEOUT_MS, "설정 응답 시간 초과. 다시 전송해주세요.")
        } else {
            tvStatus.text = "전송 실패 (연결 상태 확인)"
        }
    }

    private fun onRegister() {
        if (!cfgAcknowledged || registering) { tvStatus.text = "먼저 설정 전송을 완료해주세요."; return }
        if (!bleManager.isReady()) {
            tvStatus.text = "지팡이가 연결되어 있지 않습니다"
            return
        }
        val ok = bleManager.sendCommand("REG\n")
        tvStatus.text = if (ok) "카드 등록 요청 중..." else "전송 실패"
        if (ok) { registering = true; btnSend.isEnabled = false; btnRegister.isEnabled = false
            armTimeout(RfidConfig.ACK_TIMEOUT_MS, "등록 응답 시간 초과. 다시 시도해주세요.") }
    }

    override fun onStatusChanged(connected: Boolean) {
        if (!viewAlive) return
        if (!connected) {
            ++requestToken; cfgAcknowledged = false; registering = false; awaitingConfig = false
            tvStatus.text = "지팡이 연결이 끊겼습니다. 연결 후 설정을 다시 전송해주세요."
        }
        btnSend.isEnabled = connected
        btnRegister.isEnabled = connected && cfgAcknowledged && !registering
    }
    private fun armTimeout(delayMs: Long, message: String) {
        val token = ++requestToken
        uiHandler.postDelayed({
            if (viewAlive && token == requestToken) {
                registering = false; awaitingConfig = false
                tvStatus.text = message
                btnSend.isEnabled = bleManager.isReady()
                btnRegister.isEnabled = bleManager.isReady() && cfgAcknowledged
            }
        }, delayMs)
    }
    private fun endRegistration(message: String) {
        ++requestToken; registering = false
        tvStatus.text = message
        btnSend.isEnabled = bleManager.isReady()
        btnRegister.isEnabled = bleManager.isReady() && cfgAcknowledged
    }
    override fun onMessageReceived(message: String) {}

    override fun onRfidResponse(line: String) {
        activity?.runOnUiThread { if (viewAlive) handleRfidResponse(line) }
    }

    private fun handleRfidResponse(line: String) {
        val parts = line.split(",")
        when (parts.getOrNull(0)) {
            "ACK" -> if (parts.getOrNull(1) == "CFG" && awaitingConfig) {
                ++requestToken; awaitingConfig = false; cfgAcknowledged = true
                tvStatus.text = "대기 설정 전송 완료. 카드 등록을 진행하세요."
                btnSend.isEnabled = bleManager.isReady()
                btnRegister.isEnabled = bleManager.isReady()
            }
            "ERR" -> if (parts.getOrNull(1) == "CFG" || parts.getOrNull(1) == "QUEUE" || parts.getOrNull(1) == "COMMAND") {
                ++requestToken; awaitingConfig = false; cfgAcknowledged = false
                endRegistration("설정 오류: ${parts.getOrNull(2) ?: "UNKNOWN"}")
            }
            "REG" -> when (parts.getOrNull(1)) {
                "WAIT" -> {
                    registering = true; btnSend.isEnabled = false; btnRegister.isEnabled = false
                    val seconds = parts.getOrNull(2)?.toLongOrNull()?.coerceIn(1L, 60L) ?: 15L
                    tvStatus.text = "카드를 지팡이에 대주세요 (${seconds}초)"
                    armTimeout(seconds * 1000 + RfidConfig.ACK_TIMEOUT_MS, "등록 결과 응답 시간 초과. 카드 상태를 확인해주세요.")
                }
                "OK" -> {
                    val mode = if (parts.getOrNull(3) == "UPDATE") "덮어쓰기" else "신규"
                    endRegistration("등록 완료 ($mode): ${parts.getOrNull(4) ?: ""}")
                }
                "TIMEOUT" -> endRegistration("시간 초과. 다시 시도해주세요.")
                "NOCFG" -> { cfgAcknowledged = false; endRegistration("먼저 설정을 전송해주세요.") }
                "NO_RFID" -> endRegistration("RFID 모듈이 준비되지 않았습니다. 연결 후 다시 시도해주세요.")
                "FULL" -> endRegistration("등록 가능 인원을 초과했습니다 (최대 500명).")
                "FSERR" -> endRegistration("저장소 오류입니다. 시리얼에서 저장소 상태를 확인해주세요.")
                "CANCEL" -> endRegistration("등록 취소: ${parts.getOrNull(2) ?: "USER"}")
            }
            "APPLY" -> tvStatus.text = "등록된 카드 인식: ${parts.getOrNull(2) ?: ""}"
            "UNREG" -> tvStatus.text = "등록되지 않은 카드입니다. 기본 모드로 전환되었습니다."
        }
    }

    override fun onDestroyView() {
        if (registering && bleManager.isReady()) bleManager.sendCommand("REG_CANCEL")
        viewAlive = false; ++requestToken
        uiHandler.removeCallbacksAndMessages(null)
        bleManager.removeListener(this)
        super.onDestroyView()
    }
}
`````

## 파일: lib/VL53L4CX/src/vl53l4cx_class.cpp

SHA-256: `93825c981f564dc8d204862f89a346b77f1e83752026923c7d8dd97c55bb985b`

`````cpp

// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/******************************************************************************
 * Copyright (c) 2020, STMicroelectronics - All Rights Reserved

 This file is part of VL53L4CX and is dual licensed,
 either GPL-2.0+
 or 'BSD 3-clause "New" or "Revised" License' , at your option.
 ******************************************************************************
 */

/* Includes */
#include "vl53l4cx_class.h"

#ifndef DEFAULT_I2C_BUFFER_LEN
  #ifdef BUFFER_LENGTH
    #define DEFAULT_I2C_BUFFER_LEN  BUFFER_LENGTH
  #else
    #define DEFAULT_I2C_BUFFER_LEN  32
  #endif
#endif

/* Write and read functions from I2C */

VL53L4CX_Error VL53L4CX::VL53L4CX_WriteMulti(VL53L4CX_DEV Dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
  int  status;

  status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, pdata, (uint16_t)count);
  return status;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_ReadMulti(VL53L4CX_DEV Dev, uint16_t index, uint8_t *pdata, uint32_t count)
{
  int status;

  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, pdata, (uint16_t)count);

  return status;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_WrByte(VL53L4CX_DEV Dev, uint16_t index, uint8_t data)
{
  int  status;

  status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, &data, 1);
  return status;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_WrWord(VL53L4CX_DEV Dev, uint16_t index, uint16_t data)
{
  int  status;
  uint8_t buffer[2];

  buffer[0] = data >> 8;
  buffer[1] = data & 0x00FF;
  status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, (uint8_t *)buffer, 2);
  return status;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_WrDWord(VL53L4CX_DEV Dev, uint16_t index, uint32_t data)
{
  int  status;
  uint8_t buffer[4];

  buffer[0] = (data >> 24) & 0xFF;
  buffer[1] = (data >> 16) & 0xFF;
  buffer[2] = (data >>  8) & 0xFF;
  buffer[3] = (data >>  0) & 0xFF;
  status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, (uint8_t *)buffer, 4);
  return status;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_RdByte(VL53L4CX_DEV Dev, uint16_t index, uint8_t *data)
{
  int  status;

  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, data, 1);

  if (status) {
    return -1;
  }

  return 0;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_RdWord(VL53L4CX_DEV Dev, uint16_t index, uint16_t *data)
{
  int  status;
  uint8_t buffer[2] = {0, 0};

  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, buffer, 2);
  if (!status) {
    *data = (buffer[0] << 8) + buffer[1];
  }
  return status;

}

VL53L4CX_Error VL53L4CX::VL53L4CX_RdDWord(VL53L4CX_DEV Dev, uint16_t index, uint32_t *data)
{
  int status;
  uint8_t buffer[4] = {0, 0, 0, 0};

  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, buffer, 4);
  if (!status) {
    *data = ((uint32_t)buffer[0] << 24) + ((uint32_t)buffer[1] << 16) + ((uint32_t)buffer[2] << 8) + (uint32_t)buffer[3];
  }
  return status;

}

VL53L4CX_Error VL53L4CX::VL53L4CX_UpdateByte(VL53L4CX_DEV Dev, uint16_t index, uint8_t AndData, uint8_t OrData)
{
  int  status;
  uint8_t buffer = 0;

  /* read data direct onto buffer */
  status = VL53L4CX_I2CRead(Dev->I2cDevAddr, index, &buffer, 1);
  if (!status) {
    buffer = (buffer & AndData) | OrData;
    status = VL53L4CX_I2CWrite(Dev->I2cDevAddr, index, &buffer, (uint16_t)1);
  }
  return status;
}

// NAVCANE patch: every failed transaction returns; no unbounded retries.
extern "C" bool navSegmentLock();
extern "C" void navSegmentUnlock();
class NavBusGuard {
 public:
  bool ok = navSegmentLock();
  ~NavBusGuard() { if (ok) navSegmentUnlock(); }
};

VL53L4CX_Error VL53L4CX::VL53L4CX_I2CWrite(uint8_t DeviceAddr, uint16_t RegisterAddr, uint8_t *pBuffer, uint16_t NumByteToWrite)
{
  uint16_t offset = 0;
  while (offset < NumByteToWrite) {
    uint16_t remaining = NumByteToWrite - offset;
    uint16_t count = remaining > 28 ? 28 : remaining;
    uint16_t reg = RegisterAddr + offset;
    NavBusGuard busGuard;
    if (!busGuard.ok) return VL53L4CX_ERROR_CONTROL_INTERFACE;
    dev_i2c->beginTransmission((DeviceAddr >> 1) & 0x7F);
    if (dev_i2c->write(uint8_t(reg >> 8)) != 1 ||
        dev_i2c->write(uint8_t(reg)) != 1 ||
        dev_i2c->write(pBuffer + offset, count) != count) {
      dev_i2c->endTransmission(true);
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    }
    if (dev_i2c->endTransmission(true) != 0)
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    offset += count;
  }
  return VL53L4CX_ERROR_NONE;
}

VL53L4CX_Error VL53L4CX::VL53L4CX_I2CRead(uint8_t DeviceAddr, uint16_t RegisterAddr, uint8_t *pBuffer, uint16_t NumByteToRead)
{
  uint16_t offset = 0;
  while (offset < NumByteToRead) {
    uint16_t remaining = NumByteToRead - offset;
    uint8_t count = remaining > 28 ? 28 : remaining;
    uint16_t reg = RegisterAddr + offset;
    NavBusGuard busGuard;
    if (!busGuard.ok) return VL53L4CX_ERROR_CONTROL_INTERFACE;
    dev_i2c->beginTransmission((DeviceAddr >> 1) & 0x7F);
    if (dev_i2c->write(uint8_t(reg >> 8)) != 1 ||
        dev_i2c->write(uint8_t(reg)) != 1) {
      dev_i2c->endTransmission(true);
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    }
    if (dev_i2c->endTransmission(false) != 0)
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    if (dev_i2c->requestFrom(uint8_t((DeviceAddr >> 1) & 0x7F), count) != count)
      return VL53L4CX_ERROR_CONTROL_INTERFACE;
    for (uint8_t n = 0; n < count; ++n) {
      if (!dev_i2c->available()) return VL53L4CX_ERROR_CONTROL_INTERFACE;
      pBuffer[offset + n] = dev_i2c->read();
    }
    offset += count;
  }
  return VL53L4CX_ERROR_NONE;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_GetTickCount(
  uint32_t *ptick_count_ms)
{

  /* Returns current tick count in [ms] */

  VL53L4CX_Error status  = VL53L4CX_ERROR_NONE;

  *ptick_count_ms = (uint32_t)millis();

  return status;
}



VL53L4CX_Error VL53L4CX::VL53L4CX_WaitUs(VL53L4CX_Dev_t *pdev, int32_t wait_us)
{
  (void)pdev;
  delayMicroseconds(wait_us);
  return VL53L4CX_ERROR_NONE;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_WaitMs(VL53L4CX_Dev_t *pdev, int32_t wait_ms)
{
  (void)pdev;
  delay(wait_ms);
  return VL53L4CX_ERROR_NONE;
}


VL53L4CX_Error VL53L4CX::VL53L4CX_WaitValueMaskEx(
  VL53L4CX_Dev_t *pdev,
  uint32_t      timeout_ms,
  uint16_t      index,
  uint8_t       value,
  uint8_t       mask,
  uint32_t      poll_delay_ms)
{

  /*
   * Platform implementation of WaitValueMaskEx V2WReg script command
   *
   * WaitValueMaskEx(
   *          duration_ms,
   *          index,
   *          value,
   *          mask,
   *          poll_delay_ms);
   */

  VL53L4CX_Error status         = VL53L4CX_ERROR_NONE;
  uint32_t     start_time_ms = 0;
  uint32_t     current_time_ms = 0;
  uint32_t     polling_time_ms = 0;
  uint8_t      byte_value      = 0;
  uint8_t      found           = 0;



  /* calculate time limit in absolute time */

  VL53L4CX_GetTickCount(&start_time_ms);

  /* remember current trace functions and temporarily disable
   * function logging
   */


  /* wait until value is found, timeout reached on error occurred */

  while ((status == VL53L4CX_ERROR_NONE) &&
         (polling_time_ms < timeout_ms) &&
         (found == 0)) {

    if (status == VL53L4CX_ERROR_NONE)
      status = VL53L4CX_RdByte(
                 pdev,
                 index,
                 &byte_value);

    if ((byte_value & mask) == value) {
      found = 1;
    }

    if (status == VL53L4CX_ERROR_NONE  &&
        found == 0 &&
        poll_delay_ms > 0)
      status = VL53L4CX_WaitMs(
                 pdev,
                 poll_delay_ms);

    /* Update polling time (Compare difference rather than absolute to
    negate 32bit wrap around issue) */
    VL53L4CX_GetTickCount(&current_time_ms);
    polling_time_ms = current_time_ms - start_time_ms;

  }


  if (found == 0 && status == VL53L4CX_ERROR_NONE) {
    status = VL53L4CX_ERROR_TIME_OUT;
  }

  return status;
}
`````

## 파일: lib/SparkFun_VL53L5CX/src/SparkFun_VL53L5CX_Library.h

SHA-256: `943f740408e9c75b8085a56ef9d927fc520c6efb887c93ae1131bfe48f12b48d`

`````cpp

/*
  This is a library written for the ST VL53L5CX Time-of-flight sensor
  SparkFun sells these at its website:
  https://www.sparkfun.com/products/18642

  Do you like this library? Help support open source hardware. Buy a board!

  Written by Ricardo Ramos  @ SparkFun Electronics, October 22nd, 2021
  This file is the core of the VL53L5CX ToF sensor library.

  This library uses ST's VL53L5CX driver and parts of Simon Levy's VL53L5CX
  Arduino library available at https://github.com/simondlevy/VL53L5/tree/main/src/st

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __SparkFun_VL53L5CX_Library__
#define __SparkFun_VL53L5CX_Library__

#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_VL53L5CX_Library_Constants.h"
#include "SparkFun_VL53L5CX_IO.h"
#include "vl53l5cx_api.h"

struct SparkFun_VL53L5CX_Error
{
    // More descriptive error than a single uint32 value.
    SF_VL53L5CX_ERROR_TYPE lastErrorCode = SF_VL53L5CX_ERROR_TYPE::NO_ERROR;

    // Value as reported back from the sensor.
    uint32_t lastErrorValue = 0;
};

class SparkFun_VL53L5CX
{
private:
    // Sensor's I2C address.
    uint8_t _i2cAddress = 0;

    // Error callback function pointer.
    // Function must accept a SF_VL53L5CX_ERROR_TYPE as errorCode and an uint32_t as errorValue.
    void (*errorCallback)(SF_VL53L5CX_ERROR_TYPE errorCode, uint32_t errorValue) = nullptr;

    // Clears the error struct to a no-error state.
    void clearErrorStruct();

public:
    SparkFun_VL53L5CX_IO* VL53L5CX_i2c = nullptr; // I2C driver object
    VL53L5CX_Configuration* Dev = nullptr;        // Sensor condfiguration struct

    // This struct holds the last error which happened (if any).
    SparkFun_VL53L5CX_Error lastError;

    // Default empty constructor.
    SparkFun_VL53L5CX(){};

    // Start up the sensor. Passing an address and Wire port instance is optional.
    bool begin(byte address = (DEFAULT_I2C_ADDR >> 1), TwoWire &wirePort = Wire);

    // Set the error callback function.
    void setErrorCallback(void (*errorCallback)(SF_VL53L5CX_ERROR_TYPE errorCode, uint32_t errorValue));

    // Returns true if the sensor is connected and replying the correct device ID and revision ID.
    bool isConnected();

    // Returns true if the sensor's address was correctly changed or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool setAddress(uint8_t newAddress);

    // Return the board address.
    uint8_t getAddress();

    // Returns true if the sensor's sampling frequency was changed accordingly or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool setRangingFrequency(uint8_t newFrequency);

    // Returns current ranging frequency.
    // If this function returns 0 an error entry will be stored in the lastError struct.
    uint8_t getRangingFrequency();

    // Returns true if the sensor's ranging mode was changed accordingly or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool setRangingMode(SF_VL53L5CX_RANGING_MODE rangingMode);

    // Returns the current ranging mode.
    SF_VL53L5CX_RANGING_MODE getRangingMode();

    // Returns true if the start ranging command was acknowledged by the sensor or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool startRanging();

    // Returns true if the stop ranging command was acknowledged by the sensor or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool stopRanging();

    // Returns true if data is ready.
    bool isDataReady();

    // Returns the current ranging resolution.
    uint8_t getResolution();

    // Returns true if the sensor's ranging resolution was changed accordingly or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool setResolution(uint8_t resolution);

    // Returns true if the ranging data was read from the sensor or false otherwise.
    // Data will be stored in the VL53L5CX_ResultsData struct passed as a pointer.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool getRangingData(VL53L5CX_ResultsData *pRangingData);

    // Returns true if the sensor's power mode was changed accordingly or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool setPowerMode(SF_VL53L5CX_POWER_MODE powerMode);

    // Returns the current power mode.
    // If this function returns SF_VL53L5CX_POWER_MODE::ERROR an error entry will be stored in the lastError struct.
    SF_VL53L5CX_POWER_MODE getPowerMode();

    // Returns true if the sensor's integration time was changed accordingly or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool setIntegrationTime(uint32_t timeMsec);

    // Returns the integration time.
    // If this function returns 0 an error entry will be stored in the lastError struct.
    uint32_t getIntegrationTime();

    // Returns true if the sensor's integration time was changed accordingly or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool setSharpenerPercent(uint8_t percent);

    // Get the sharpener percentage, ranging from 0 (disabled) to 99 (maximum).
    // If this function returns 0xff an error entry will be stored in the lastError struct.
    uint8_t getSharpenerPercent();

    // Returns true if the target order was set or false otherwise.
    // If this function returns false an error entry will be stored in the lastError struct.
    bool setTargetOrder(SF_VL53L5CX_TARGET_ORDER order);

    // Gets target order.
    // If this function returns SF_VL53L5CX_TARGET_ORDER::ERROR an error entry will be stored in the lastError struct.
    SF_VL53L5CX_TARGET_ORDER getTargetOrder();

    // Gets I2C maximum packet size.
    uint8_t getWireMaxPacketSize();

    // Sets I2C maximum packet size.
    void setWireMaxPacketSize(uint8_t newSize = I2C_BUFFER_SIZE);
};
#endif
`````

## 파일: lib/SparkFun_VL53L5CX/src/SparkFun_VL53L5CX_Library.cpp

SHA-256: `efc8b751430839c018115c192994d266e0f5bc1723d1a5b032e25848756e8442`

`````cpp
/*
  This is a library written for the ST VL53L5CX Time-of-flight sensor
  SparkFun sells these at its website:
  https://www.sparkfun.com/products/18642

  Do you like this library? Help support open source hardware. Buy a board!

  Written by Ricardo Ramos  @ SparkFun Electronics, October 22nd, 2021
  This file implements the VL53L5CX core library class.

  This library uses ST's VL53L5CX driver and parts of Simon Levy's VL53L5CX
  Arduino library available at https://github.com/simondlevy/VL53L5/tree/main/src/st

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "SparkFun_VL53L5CX_Library.h"
#include "SparkFun_VL53L5CX_IO.h"
#include "vl53l5cx_api.h"
#include <new>

void SparkFun_VL53L5CX::clearErrorStruct()
{
    // Set last error struct to no-error condition
    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::NO_ERROR;
    lastError.lastErrorValue = 0;
}

bool SparkFun_VL53L5CX::begin(byte address, TwoWire &wirePort)
{
    clearErrorStruct();

    // NAVCANE: reuse allocations on recovery; upstream leaked one pair per begin().
    if (!VL53L5CX_i2c) VL53L5CX_i2c = new (std::nothrow) SparkFun_VL53L5CX_IO();
    if (!Dev) Dev = new (std::nothrow) VL53L5CX_Configuration();
    if (!VL53L5CX_i2c || !Dev) return false;
    memset(Dev, 0, sizeof(*Dev));

    Dev->platform.VL53L5CX_i2c = VL53L5CX_i2c;

    bool ready = VL53L5CX_i2c->begin(address, wirePort);
    uint8_t result = 0;
    uint8_t deviceId = 0;
    uint8_t revisionId = 0;

    if (!ready)
    {
        SAFE_CALLBACK(errorCallback, SF_VL53L5CX_ERROR_TYPE::I2C_INITIALIZATION_ERROR, UNKNOWN_ERROR_VALUE);
        return false;
    }

    VL53L5CX_i2c->writeSingleByte(0x7fff, 0x00);
    deviceId = VL53L5CX_i2c->readSingleByte(0x00);
    revisionId = VL53L5CX_i2c->readSingleByte(0x01);
    VL53L5CX_i2c->writeSingleByte(0x7fff, 0x02);

    if ((revisionId != REVISION_ID) || (deviceId != DEVICE_ID))
    {
        lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::INVALID_DEVICE;
        lastError.lastErrorValue = UNKNOWN_ERROR_VALUE;
        SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
        return false;
    }

    result = vl53l5cx_init(Dev);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::DEVICE_INITIALIZATION_ERROR;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

void SparkFun_VL53L5CX::setErrorCallback(void (*_errorCallback)(SF_VL53L5CX_ERROR_TYPE errorCode, uint32_t errorValue))
{
    errorCallback = _errorCallback;
}

bool SparkFun_VL53L5CX::isConnected()
{
    clearErrorStruct();

    uint8_t result = 0;

    bool connected = VL53L5CX_i2c->isConnected();

    if (!connected)
    {
        SAFE_CALLBACK(errorCallback, SF_VL53L5CX_ERROR_TYPE::I2C_NOT_RESPONDING, UNKNOWN_ERROR_VALUE);
        return false;
    }

    uint8_t alive = 0;
    result = vl53l5cx_is_alive(Dev, &alive);
    if (result != 0)
    {
        SAFE_CALLBACK(errorCallback, SF_VL53L5CX_ERROR_TYPE::DEVICE_NOT_ALIVE, result);
        return false;
    }
    else
    {
        return (alive != 0);
    }
}

bool SparkFun_VL53L5CX::setAddress(uint8_t newAddress)
{
    clearErrorStruct();

    // Don't use core vl53l5cx_set_i2c_address() as it calls WrByte with new address that SparkFun driver is not yet aware of
    //uint8_t result = vl53l5cx_set_i2c_address(Dev, static_cast<uint16_t>(newAddress));

    uint8_t result = VL53L5CX_i2c->writeSingleByte(0x7fff, 0x00);
    result |= VL53L5CX_i2c->writeSingleByte(0x4, newAddress);

    if (result == 0)
    {
        VL53L5CX_i2c->setAddress(newAddress); //Tell the middle layer what our new address is
        _i2cAddress = newAddress; //Tell Arduino lib what our new address is
        vl53l5cx_set_i2c_address(Dev, static_cast<uint16_t>(newAddress)); //Tell the core what our new address is
        
        result |= VL53L5CX_i2c->writeSingleByte(0x7fff, 0x02); //Do last write with new address
        
        return true;
    }

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_CHANGE_I2C_ADDRESS;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

uint8_t SparkFun_VL53L5CX::getAddress()
{
    return _i2cAddress;
}

bool SparkFun_VL53L5CX::setRangingFrequency(uint8_t newFrequency)
{
    clearErrorStruct();

    uint8_t result = vl53l5cx_set_ranging_frequency_hz(Dev, newFrequency);
    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::INVALID_FREQUENCY_SETTING;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

uint8_t SparkFun_VL53L5CX::getRangingFrequency()
{
    clearErrorStruct();

    uint8_t frequency = 0;
    uint8_t result = vl53l5cx_get_ranging_frequency_hz(Dev, &frequency);
    if (result == 0)
    {
        return frequency;
    }
    else
    {
        lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::INVALID_FREQUENCY_SETTING;
        lastError.lastErrorValue = static_cast<uint32_t>(result);
        SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
        return 0;
    }
}

bool SparkFun_VL53L5CX::setRangingMode(SF_VL53L5CX_RANGING_MODE rangingMode)
{
    clearErrorStruct();

    uint8_t result;

    if (rangingMode == SF_VL53L5CX_RANGING_MODE::AUTONOMOUS)
        result = vl53l5cx_set_ranging_mode(Dev, VL53L5CX_RANGING_MODE_AUTONOMOUS);
    else
        result = vl53l5cx_set_ranging_mode(Dev, VL53L5CX_RANGING_MODE_CONTINUOUS);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::INVALID_RANGING_MODE;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

SF_VL53L5CX_RANGING_MODE SparkFun_VL53L5CX::getRangingMode()
{
    clearErrorStruct();

    uint8_t rangeMode = 0;
    uint8_t result = vl53l5cx_get_ranging_mode(Dev, &rangeMode);
    if (result == 0)
    {
        if (rangeMode == 0x01)
            return SF_VL53L5CX_RANGING_MODE::CONTINUOUS;
        else
            return SF_VL53L5CX_RANGING_MODE::AUTONOMOUS;
    }

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::INVALID_RANGING_MODE;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return SF_VL53L5CX_RANGING_MODE::ERROR;
}

bool SparkFun_VL53L5CX::startRanging()
{
    clearErrorStruct();

    uint8_t result = vl53l5cx_start_ranging(Dev);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_START_RANGING;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

bool SparkFun_VL53L5CX::stopRanging()
{
    clearErrorStruct();

    uint8_t result = vl53l5cx_stop_ranging(Dev);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_STOP_RANGING;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

bool SparkFun_VL53L5CX::isDataReady()
{
    clearErrorStruct();
    uint8_t dataReady = 0;

    uint8_t result = vl53l5cx_check_data_ready(Dev, &dataReady);
    if (result == 0)
        return dataReady != 0;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_GET_DATA_READY;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

uint8_t SparkFun_VL53L5CX::getResolution()
{
    clearErrorStruct();

    uint8_t resolution = 0;
    uint8_t result = vl53l5cx_get_resolution(Dev, &resolution);
    if (result == 0)
    {
        if (resolution == 64)
            return (uint8_t)SF_VL53L5CX_RANGING_RESOLUTION::RES_8X8;
        else
            return (uint8_t)SF_VL53L5CX_RANGING_RESOLUTION::RES_4X4;
    }

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_GET_RESOLUTION;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return (uint8_t)SF_VL53L5CX_RANGING_RESOLUTION::ERROR;
}

bool SparkFun_VL53L5CX::setResolution(uint8_t resolution)
{
    clearErrorStruct();

    uint8_t result = vl53l5cx_set_resolution(Dev, resolution);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_SET_RESOLUTION;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

bool SparkFun_VL53L5CX::getRangingData(VL53L5CX_ResultsData *pRangingData)
{
    clearErrorStruct();

    uint8_t result = vl53l5cx_get_ranging_data(Dev, pRangingData);
    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_GET_RANGING_DATA;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

bool SparkFun_VL53L5CX::setPowerMode(SF_VL53L5CX_POWER_MODE powerMode)
{
    clearErrorStruct();

    uint8_t powerModeValue;
    if (powerMode == SF_VL53L5CX_POWER_MODE::SLEEP)
        powerModeValue = VL53L5CX_POWER_MODE_SLEEP;
    else
        powerModeValue = VL53L5CX_POWER_MODE_WAKEUP;

    uint8_t result = vl53l5cx_set_power_mode(Dev, powerModeValue);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_SET_POWER_MODE;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

SF_VL53L5CX_POWER_MODE SparkFun_VL53L5CX::getPowerMode()
{
    clearErrorStruct();

    uint8_t powerModeValue;
    uint8_t result = vl53l5cx_get_power_mode(Dev, &powerModeValue);

    if (result == 0)
    {
        switch (powerModeValue)
        {
        case 0:
            return SF_VL53L5CX_POWER_MODE::SLEEP;
            break;

        case 1:
            return SF_VL53L5CX_POWER_MODE::WAKEUP;
            break;

        default:
            return SF_VL53L5CX_POWER_MODE::ERROR;
            break;
        }
    }

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_GET_POWER_MODE;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return SF_VL53L5CX_POWER_MODE::ERROR;
}

bool SparkFun_VL53L5CX::setIntegrationTime(uint32_t timeMsec)
{
    clearErrorStruct();

    // Does not allow invalid values to be sent to the device
    if (timeMsec < 2 || timeMsec > 1000)
    {
        lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::INVALID_INTEGRATION_TIME;
        lastError.lastErrorValue = UNKNOWN_ERROR_VALUE;
        SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
        return false;
    }

    uint8_t result = vl53l5cx_set_integration_time_ms(Dev, timeMsec);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_SET_INTEGRATION_TIME;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

uint32_t SparkFun_VL53L5CX::getIntegrationTime()
{
    clearErrorStruct();
    uint32_t integrationTime;
    uint8_t result = vl53l5cx_get_integration_time_ms(Dev, &integrationTime);

    if (result == 0)
    {
        return integrationTime;
    }

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_GET_INTEGRATION_TIME;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return result;
}

bool SparkFun_VL53L5CX::setSharpenerPercent(uint8_t percent)
{
    clearErrorStruct();

    if (percent > 99)
    {
        lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::INVALID_SHARPENER_VALUE;
        lastError.lastErrorValue = UNKNOWN_ERROR_VALUE;
        SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    }

    uint8_t result = vl53l5cx_set_sharpener_percent(Dev, percent);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_SET_SHARPENER_VALUE;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

uint8_t SparkFun_VL53L5CX::getSharpenerPercent()
{
    clearErrorStruct();

    uint8_t percent;
    uint8_t result = vl53l5cx_get_sharpener_percent(Dev, &percent);

    if (result == 0)
        return percent;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_GET_SHARPENER_VALUE;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return 0xff;
}

bool SparkFun_VL53L5CX::setTargetOrder(SF_VL53L5CX_TARGET_ORDER order)
{
    clearErrorStruct();

    uint8_t orderValue;
    if (order == SF_VL53L5CX_TARGET_ORDER::CLOSEST)
        orderValue = VL53L5CX_TARGET_ORDER_CLOSEST;
    else
        orderValue = VL53L5CX_TARGET_ORDER_STRONGEST;

    uint8_t result = vl53l5cx_set_target_order(Dev, orderValue);

    if (result == 0)
        return true;

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_SET_TARGET_ORDER;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return false;
}

SF_VL53L5CX_TARGET_ORDER SparkFun_VL53L5CX::getTargetOrder()
{
    clearErrorStruct();

    uint8_t orderValue;
    uint8_t result = vl53l5cx_get_target_order(Dev, &orderValue);

    if (result == 0)
    {
        if (orderValue == VL53L5CX_TARGET_ORDER_CLOSEST)
            return SF_VL53L5CX_TARGET_ORDER::CLOSEST;
        else
            return SF_VL53L5CX_TARGET_ORDER::STRONGEST;
    }

    lastError.lastErrorCode = SF_VL53L5CX_ERROR_TYPE::CANNOT_GET_TARGET_ORDER;
    lastError.lastErrorValue = static_cast<uint32_t>(result);
    SAFE_CALLBACK(errorCallback, lastError.lastErrorCode, lastError.lastErrorValue);
    return SF_VL53L5CX_TARGET_ORDER::ERROR;
}

uint8_t SparkFun_VL53L5CX::getWireMaxPacketSize()
{
    return VL53L5CX_i2c->getMaxPacketSize();
}

void SparkFun_VL53L5CX::setWireMaxPacketSize(uint8_t newSize)
{
    VL53L5CX_i2c->setMaxPacketSize(newSize);
}
`````

## 파일: lib/SparkFun_VL53L5CX/src/SparkFun_VL53L5CX_IO.cpp

SHA-256: `ebc869ce5ba735e0ea308f1218fb7f40e376632bda5db83d286c776f12ddfb1f`

`````cpp
/*
  This is a library written for the ST VL53L5CX Time-of-flight sensor
  SparkFun sells these at its website:
  https://www.sparkfun.com/products/18642

  Do you like this library? Help support open source hardware. Buy a board!

  Written by Ricardo Ramos  @ SparkFun Electronics, October 22nd, 2021
  This file implements the VL53L5CX I2C driver class.

  This library uses ST's VL53L5CX driver and parts of Simon Levy's VL53L5CX
  Arduino library available at https://github.com/simondlevy/VL53L5/tree/main/src/st

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "SparkFun_VL53L5CX_IO.h"
#include "SparkFun_VL53L5CX_Library_Constants.h"

bool SparkFun_VL53L5CX_IO::begin(byte address, TwoWire &wirePort)
{
    _address = address;
    _i2cPort = &wirePort;
    return isConnected();
}

bool SparkFun_VL53L5CX_IO::isConnected()
{
    _i2cPort->beginTransmission(_address);
    if (_i2cPort->endTransmission() != 0)
        return (false);
    return (true);
}

void SparkFun_VL53L5CX_IO::setAddress(uint8_t newAddress)
{
    _address = newAddress;
}

// Must be able to write 32,768 bytes at a time
uint8_t SparkFun_VL53L5CX_IO::writeMultipleBytes(uint16_t registerAddress, uint8_t *buffer, uint16_t bufferSize)
{
    // Chunk I2C transactions into limit of 32 bytes (or wireMaxPacketSize)
    uint8_t i2cError = 0;
    uint32_t startSpot = 0;
    uint32_t bytesToSend = bufferSize;
    while (bytesToSend > 0)
    {
        uint32_t len = bytesToSend;
        if (len > uint32_t(wireMaxPacketSize - 2)) // Allow 2 byte for register address
            len = (wireMaxPacketSize - 2);

        _i2cPort->beginTransmission((uint8_t)_address);
        _i2cPort->write(highByte(registerAddress));
        _i2cPort->write(lowByte(registerAddress));

        // TODO write a subsection of the buffer rather than byte wise
        for (uint16_t x = 0; x < len; x++)
            _i2cPort->write(buffer[startSpot + x]); // Write a portion of the payload to the bus

        i2cError = _i2cPort->endTransmission(); // Release bus because we are writing the address each time
        if (i2cError != 0)
            return (i2cError); // Sensor did not ACK

        startSpot += len; // Move the pointer forward
        bytesToSend -= len;
        registerAddress += len; // Move register address forward
    }
    return (i2cError);
}

uint8_t SparkFun_VL53L5CX_IO::readMultipleBytes(uint16_t registerAddress, uint8_t *buffer, uint16_t bufferSize)
{
    uint8_t i2cError = 0;

    // Write address to read from
    _i2cPort->beginTransmission(_address);
    _i2cPort->write(highByte(registerAddress));
    _i2cPort->write(lowByte(registerAddress));
    i2cError = _i2cPort->endTransmission(false); // Do not release bus
    if (i2cError != 0)
        return (i2cError);

    // Read bytes up to max transaction size
    uint16_t bytesToReadRemaining = bufferSize;
    uint16_t offset = 0;
    while (bytesToReadRemaining > 0)
    {
        // Limit to 32 bytes or whatever the buffer limit is for given platform
        uint16_t bytesToRead = bytesToReadRemaining;
        if (bytesToRead > wireMaxPacketSize)
            bytesToRead = wireMaxPacketSize;

        size_t received = _i2cPort->requestFrom((uint8_t)_address, (uint8_t)bytesToRead);
        if (received == bytesToRead && _i2cPort->available() >= bytesToRead)
        {
            for (uint16_t x = 0; x < bytesToRead; x++)
                buffer[offset + x] = _i2cPort->read();
        }
        else
            return (1); // NAVCANE: nonzero means failed/short read

        offset += bytesToRead;
        bytesToReadRemaining -= bytesToRead;
    }

    return (0); // Success
}

uint8_t SparkFun_VL53L5CX_IO::readSingleByte(uint16_t registerAddress)
{
    _i2cPort->beginTransmission(_address);
    _i2cPort->write(highByte(registerAddress));
    _i2cPort->write(lowByte(registerAddress));
    _i2cPort->endTransmission();
    _i2cPort->requestFrom(_address, 1U);
    return _i2cPort->read();
}

uint8_t SparkFun_VL53L5CX_IO::writeSingleByte(uint16_t registerAddress, uint8_t const value)
{
    _i2cPort->beginTransmission(_address);
    _i2cPort->write(highByte(registerAddress));
    _i2cPort->write(lowByte(registerAddress));
    _i2cPort->write(value);
    return _i2cPort->endTransmission();
}
`````

## 파일: lib/SparkFun_VL53L5CX/src/platform.cpp

SHA-256: `39c23a772cd169093daba5e4f219200f5855c7f39c0a8a896d04a2b554163e46`

`````cpp
/*******************************************************************************
* Copyright (c) 2020, STMicroelectronics - All Rights Reserved
*
* This file is part of the VL53L5CX Ultra Lite Driver and is dual licensed,
* either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0081
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, the VL53L5CX Ultra Lite Driver may be distributed under the
* terms of 'BSD 3-clause "New" or "Revised" License', in which case the
* following provisions apply instead of the ones mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
*******************************************************************************/


#include "SparkFun_VL53L5CX_Library.h"
#include "SparkFun_VL53L5CX_IO.h"
#include "platform.h"

uint8_t RdByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_value)
{
	return p_platform->VL53L5CX_i2c->readMultipleBytes(RegisterAdress, p_value, 1);
}

uint8_t WrByte(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t value)
{
	return p_platform->VL53L5CX_i2c->writeSingleByte(RegisterAdress, value);
}

uint8_t WrMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_values, uint32_t size)
{
	return p_platform->VL53L5CX_i2c->writeMultipleBytes(RegisterAdress, p_values, size);
}

uint8_t RdMulti(VL53L5CX_Platform *p_platform, uint16_t RegisterAdress, uint8_t *p_values, uint32_t size)
{
	return p_platform->VL53L5CX_i2c->readMultipleBytes(RegisterAdress, p_values, size);
}

void SwapBuffer(uint8_t *buffer, uint16_t size)
{
	uint32_t i, tmp;

	/* Example of possible implementation using <string.h> */
	for (i = 0; i < size; i = i + 4)
	{
		tmp = (buffer[i] << 24) | (buffer[i + 1] << 16) | (buffer[i + 2] << 8) | (buffer[i + 3]);
		memcpy(&(buffer[i]), &tmp, 4);
	}
}

uint8_t WaitMs(VL53L5CX_Platform *p_platform, uint32_t TimeMs)
{
	delay(TimeMs);
	return 0;
}
`````

## 파일: docs/APP_INTEGRATION.md

SHA-256: `a5f243c28811b4c8e74bd607d382c3fda56571c17501cf01de7a0677930e4da2`

`````markdown
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
`````

## 파일: docs/CHANGES.md

SHA-256: `e749e5731a69aad7630d1d23cb8bcb8af08915ef7ad56bdcae91c38c4878c8de`

`````markdown
# 수정 기록 / 설계 결정

## v4에서 추가한 수정

1. 낙상 보관을 무제한에서 5분 미만으로 변경했습니다. BLE 미연결과 CCCD OFF 상태에서도 만료하고, 송신 직전에도 마감을 확인합니다. 발생 시각과 보관 플래그는 짧은 critical section에서 함께 갱신/검사합니다. BLE 만료가 즉시 낙상 진동을 취소하지 않습니다.
2. 센서의 600ms 데이터 신선도와 2500ms 재초기화 조건을 분리했습니다. `nav::ObstacleRecovery`를 실제 L5CX 태스크에서 사용해 정상 측정 중 프레임 정지는 즉시 복구, 초기화 실패는 종료 후 5초 간격으로 관리합니다. millis wrap 경계도 검사했습니다.
3. L5CX 전용 Wire timeout=500ms와 공유 segment timeout=25ms를 별도 설정으로 분리했습니다. 실제 초기화 지연과 프레임 지연은 현장에서 측정해야 합니다.
4. 상태 조회와 계단 전환 메시지를 `계단 감지`로 통일했습니다. 해제 `STAIR_CLEARED`는 유지합니다.
5. 메인의 fault buzz 대상 비트를 0x07로 제한해 확장모듈의 별도 경고를 다시 시작하지 않습니다.
6. 실제 main.cpp의 consumeSensors/status를 PC에서 컴파일·실행하는 검사를 추가했습니다. 확장모듈 진동 중복, 기본 센서 새 고장, 599/600ms 신선도, 계단 메시지와 상태 동기화를 확인합니다. 설정/루프/시리얼 전체 MCU 실행을 대체하는 검사는 아닙니다.
7. Claude diff에서 빠졌던 상수 정의를 포함했고, 10분으로 제시됐던 보관 정책은 사용자 요청에 따라 5분으로 줄였습니다. 이 폴더의 전체 코드가 전달 대상이며 외부 diff를 추가 적용하지 않습니다.

## v3에서 추가한 수정

1. 상부 경고 거리 보존과 실제 expansion.cpp 테스트를 추가했습니다. 해제 확인 전에 진동이 먼저 사라지던 문제를 수정했습니다. 비정상 응답은 연속 판정을 끊고 600ms 신선도 제한을 넘기지 않습니다.
2. CFG에 BLE 세대를 기록하고 카드 저장 직전에 확인합니다. 빠른 재연결로 루프가 단절을 관측하지 못해도 이전 설정을 등록하지 않습니다. 루프 시작에서도 세대를 정리합니다.
3. REG 마감 검사를 카드 처리 앞으로 옮기고 15초 경계와 millis wrap 회귀 테스트를 추가했습니다.
4. 낙상 후보 600ms 만료를 충격 인정 전에 검사합니다. 두 버전 공통 결함을 고쳤습니다.
5. VL53L5CX 버스 재시작·클록/STOP 복구, LPn 준비 대기, 초기화 종료 이후 재시도 간격을 추가했습니다.
6. 주기 상태 확인의 단발 NACK를 완화하되 MPU의 확인된 잘못된 설정은 즉시 중지합니다. 데이터 신선도 제한은 유지합니다.
7. 현재 포고핀 상태를 BLE 준비/GET_STATUS 시 다시 보내고 초기화 중 상태를 구분합니다.

## 앞선 전달본에서 고친 점

1. 초음파 30ms 제한을 60ms로 바꾸고, 완료된 정상 긴 펄스와 무응답·HIGH 고정을 구분했습니다. 먼 정상 응답은 고장 누적에서 제외합니다. 펄스 종료가 없다는 사실만으로 정상 판정하지 않습니다.
2. 저장소 자동 삭제를 유지하지 않고, 마운트 실패/파일 손상 때 시리얼의 명시적 파일시스템 초기화 명령을 제공했습니다. 정상 저장소는 초기화를 거부합니다. 전체 플래시 삭제를 요구하지 않습니다.
3. `포고핀 분리상태`, `포고핀 재결합`을 복원했습니다. 센서별 고장은 `SENSOR:` 상태로 별도로 표시합니다. 공통 분절을 개별 센서 한 개의 고장만으로 분리 판정하지 않습니다.
4. MPU와 계단 센서 각각에 3초 대기+5회 연속 안정 확인을 추가했습니다. 한 센서가 고장이어도 정상 센서의 초기화와 측정을 유지합니다.
5. 슬롯의 전기적 출력 허가와 논리적 장착 상태를 분리했습니다. 순간 ID 이상은 high-Z/일시 중지이며, 분리 이벤트는 100ms 지속 시 확정합니다. 원래 ID 복귀 30ms 후 재개합니다.
6. 계단 감지/해제 메시지를 각각 하나로 통일했습니다. 초음파는 UPPER 이벤트를 사용하고 앱에 상부 표시를 추가했습니다.
7. CFG 즉시 적용을 제거했습니다. 등록 성공/등록 카드 인식 시만 활성 설정이 바뀝니다. 새 CFG는 기존 REG를 취소하고 새 REG를 요구합니다.
8. LED를 원래 정상/활성 의미와 Q0 L5CX/Q1 MPU/Q2 L4CX/Q3 BLE/Q4 YOLO 순서로 복원했습니다.
9. 기존 앱에는 한 notify=한 이벤트를 유지했습니다. 교체 앱은 STREAM_ON을 요청하고 UTF-8 바이트를 줄 단위로 조립합니다. MTU가 작은 앱→ESP32 쓰기도 분할합니다.
10. MPU/계단/L5CX를 세 태스크로 분리했습니다. MPU를 15ms 목표의 절대 주기로 예약하고 ST 드라이버의 28바이트 전송마다 공유 버스 mutex를 사용합니다. 센서 초기화 전체를 잠그지 않습니다.
11. 낙상 패턴을 계단 경고가 취소하던 문제를 고쳤습니다. 낙상 패턴 동안 브레이크는 계속 계단 상태를 따릅니다.
12. 앱의 CCCD/GATT 상태 코드·콜백 시간 초과·명령 전송 실패를 처리합니다. 실패한 GATT 쓰기는 수신 여부가 모호하므로 CFG/REG 전체를 자동 재전송하지 않습니다.
13. RFID 화면은 추가 수신자로 등록합니다. 주 수신자와 낙상 처리 경로를 덮어쓰지 않습니다. ACK,CFG만 설정 성공으로 처리하고 모든 REG 오류/취소·타임아웃을 표시합니다.
14. 폰 연결이 바뀌면 대기 CFG와 등록 상태를 정리하고, 이전 연결의 큐·수신 조각을 실행하지 않습니다. 이미 적용한 사용자 설정은 유지합니다.
15. 거리의 3회 이동 평균, 상부 120/130cm 히스테리시스·연속 2회 판정을 반영했습니다. 임시 보정/카드별 거리표의 동작을 문서화했습니다.
16. 센서 모델 ID의 두 read 호출을 별도 문장으로 실행해 바이트 읽기 순서를 명시했습니다.

## 유지한 정책

- 계단 센서가 정상 프레임에서 바닥을 못 찾거나 임계값보다 멀면 위험 경고입니다. 센서 통신 단절/600ms 프레임 없음은 센서 고장으로 구분하고 브레이크를 해제합니다.
- 브레이크 0°/20°, 키별 실측표 미확정에 따른 공통 170cm 기본값, GPIO 핀 배치를 유지했습니다.
- 슬롯 7핀, RC522 RST→모듈 3.3V, 유한 소프트 리셋 재시도 후 FAULT, 재장착 복구를 유지했습니다.
- 저장소 CRC·임시 파일 검증·rename과 손상 파일 보존을 유지했습니다. 테스트는 파일 쓰기/교체 실패를 주입한 시뮬레이션이며 실제 전원 차단 검증과는 다릅니다.

## 드라이버 출처와 수정

- STMicroelectronics / stm32duino VL53L4CX 1.1.0, commit `8ef5e558f4617ac52f559563e9ffe285a8b345f8`.
  https://github.com/stm32duino/VL53L4CX
  `src/vl53l4cx_class.cpp`: 실패 시 무한 재시도 제거, 짧은 읽기/쓰기 오류 반환, 28바이트 전송, 전송 단위 RAII mutex. `navSegmentLock`/`navSegmentUnlock`은 프로젝트의 sensors.cpp에서 제공합니다.
- SparkFun VL53L5CX 1.0.3.
  https://github.com/sparkfun/SparkFun_VL53L5CX_Arduino_Library
  Library.h/.cpp, IO.cpp, platform.cpp: begin 재할당 누적 제거, 모델/revision 검증, 짧은 수신과 RdByte 실패 코드 수정.
- MPU6050 레지스터: WHO_AM_I 0x75, 가속도 0x3B, ±8g/4096 LSB/g, DLPF 설정을 직접 사용하며 ACK와 수신 바이트 수를 확인합니다.
  https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map.pdf

원 드라이버 저작권 표시와 라이선스를 포함했습니다. Android 변경은 첨부한 세 파일의 API와 화면 구조를 기준으로 작성했습니다.
`````

## 파일: docs/CLAUDE_COMPARISON.md

SHA-256: `a03fb17568665527cde3d854f8172c42d8bcf6801f12faa704d8d2424eeb0fbd`

`````markdown
# 현재 v4.2 추가 결정

사용자 요청에 따라 센서·슬롯·확장모듈의 고장 경고 진동을 끕니다. 한국어 로그와 BLE 상태, 자동 복구는 유지합니다. 낙상·계단·장애물 진동을 유지합니다. 아래 표는 v4 당시 결정 기록입니다.

# v3 후속 검토와 v4 결정

비교 대상은 실제 NAVCANE_Integrated_v3 소스와 첨부 CLAUDE_CHANGES_vs_GPT_v3.diff입니다. 이전 14개 파일 비교 대상은 클로드 v2이며, NAVCANE_BEST 전체의 비교 결과로 확대하지 않습니다. 해당 과거 기록은 CLAUDE_V2_COMPARISON.md로 구분했습니다.

| 항목 | v3 / 클로드 제안 | v4 결정 |
|---|---|---|
| 장애물 프레임 손실 | v3는 600ms에 드라이버 종료 후 추가 5초 대기. 제안은 2.5초에 즉시 복구 | 600ms 이후 거리값 사용 중단, 측정은 계속 확인. 2.5초 무프레임이면 즉시 재초기화. 실패 후만 5초 대기 |
| I2C timeout | v3는 두 버스 모두 25ms. 기존 정상 동작 L5CX 코드 값은 500ms | 전용 L5CX=500ms, MPU·계단 공유 버스=25ms |
| 계단 이벤트 | v3 STAIR_DETECTED, 제안은 한글 | 계단 감지 한글 유지. 해제 STAIR_CLEARED 유지 |
| 확장 고장 진동 | 모듈 처리와 메인 bit3 처리로 재시작 가능 | 메인은 센서 비트 0x07만 경고, 모듈은 자체 경고 |
| 보류 낙상 | v3 무제한, diff 주석은 10분 제한 | 사용자 요청을 반영해 5분 미만만 전송. 미연결에서도 만료. 새 사건은 새 시각부터 계산 |
| 적용 완결성 | diff에 두 상수 정의가 없고 테스트 hunk는 실제 v3와 문맥 차이 | 필요한 정의·전체 수정 소스·현재 테스트를 폴더에 포함 |

클로드 diff의 ①~③은 타당한 수정입니다. 낙상 시간 제한은 위치를 저장하는 기능이 아니라 지연 알림을 제한하는 정책입니다. v4는 발생 시각과 대기 상태를 한 번에 잠가 갱신하고, 단절/알림 미활성/송신 시점에서 만료를 검사합니다. 만료된 사건은 재연결해도 FALL_DETECTED를 보내지 않습니다. 5분 미만 이벤트가 발생 장소와 같은 위치로 전달된다는 보장은 없습니다.

v4의 유지된 장점은 3센서 태스크, 전송 단위 공유 I2C mutex, 독립 센서 복구, 초음파 긴 정상 펄스/HIGH 고정 구분, 상부 확인 전 거리 유지, 낙상 진동 우선, CFG 대기/세대 검사, 손상 파일 보존, 앱 다중 수신자와 줄 단위 BLE 조립입니다.

검증은 실제 C++ 구현의 PC 회귀 테스트, ESP32 빌드, Kotlin API 컴파일과 JVM 메시지 조립입니다. 실제 낙상 정확도, 센서 인식률, 전원 차단 내구성과 전체 보호자 앱 전달은 실물/전체 앱 시험이 필요합니다.
`````

## 파일: docs/CLAUDE_V2_COMPARISON.md

SHA-256: `4bef618e18ca8fa430804fdb0ea4dd05b6109af45feeb14968beb02490c717a7`

`````markdown
> 과거 v3 작성 당시 클로드 제공본(v2)을 비교한 기록입니다. 이후 NAVCANE_BEST 전체를 검토했다는 뜻이 아닙니다. 최신 동작은 README와 CLAUDE_COMPARISON.md를 기준으로 합니다.

# 클로드 제공본 / 이전 전달본 / 통합본 v3 비교

## 판단

제공된 코드 기준으로 **이전 전달본(NAVCANE_Final)의 3태스크·수정 드라이버·저장소 보호·앱 교체 구조가 기반으로 더 적합**합니다. 클로드 제공본에는 경고 거리 유지, 버스 재시작, 상태 동기화의 장점이 있습니다. 이 장점을 반영하고 양쪽의 공통 결함도 고친 것이 NAVCANE_Integrated_v3입니다.

이는 코드 구조, 실행한 PC 테스트와 통합본 ESP32 빌드에 근거한 판단입니다. 센서 인식률·낙상 민감도/특이도·실외 장애물 감지 성능이 더 높다는 실물 비교 결과는 아닙니다. 170cm와 낙상 가속도/시간 기준은 실측 검증을 거친 값으로 주장하지 않습니다.

비교 대상은 사용자가 제공한 텍스트의 14개 프로젝트 파일과 ST 드라이버 diff입니다. 첨부 원문은 `reference/CLAUDE_ORIGINAL.md`에 보관했습니다. 이전 전달본 전체와 새 통합본을 비교했습니다. 클로드가 과거에 지적했던 초기 전달본 문제 중 일부는 이전 전달본에서 이미 수정되어 있으므로 이번 비교에서 구분합니다.

## 실제 구현 비교

| 항목 | 클로드 제공본 | 이전 전달본 | v3 결정 |
|---|---|---|---|
| MPU와 계단 초기화 | segmentTask 한 태스크 안에서 MPU + 계단 init 실행. ToF init 중 MPU 읽기 지연 가능 | MPU/계단/L5CX 3태스크, 버스 전송 단위 mutex | 3태스크 유지. 물리 공유 버스 고장·OS·플래시 지연까지 없어진다는 보장은 없음 |
| 개별 고장 | 준비/복구 플래그 독립, 실행 태스크는 공유 | 플래그와 실행 태스크 독립 | 정상 센서 계속 사용 |
| 상태 확인 흔들림 | 주기 확인 2회 실패 확인 | 주기 확인 1회 실패로 초기화 대기 진입 | 단발 상태 확인 NACK 완화. 확인된 잘못된 MPU 설정은 즉시 중지. 데이터 신선도는 별도 검사 |
| 포고핀 안정 | 200ms 간격 5회. 초기 3초 대기 없음 | 3초 + 200ms 간격 5회 | 기존 안정 확인 유지 |
| 슬롯 ID | 5ms 주기 3회 불일치 후 출력 중지/논리 분리 | 첫 불일치 출력 차단, 100ms 논리 분리, 30ms 복귀 재개 | 출력 보호와 RC522 무리셋 재개 유지 |
| 초음파 | 30ms 이후 상승/계속 HIGH는 정상 없음으로 분류 | 60ms 안에 끝난 긴 펄스는 FAR, HIGH 고정/무응답 구분 | 이전 방식 유지. 실제 부품의 범위 밖 펄스 지속 시간은 실물 측정 필요 |
| 상부 경고 거리 | 경고 거리를 해제 확인까지 유지. 중간 대역이 연속 카운터를 끊지 않음 | 중간 대역에서 카운터 리셋. 첫 FAR에서 경고는 남지만 전달 거리가 -1로 바뀌어 진동 정지 가능 | 경고 거리를 유지하고 연속 확인 리셋도 유지. 비정상 응답은 해제 증거로 쓰지 않으며 600ms 신선도 제한 |
| 낙상 600ms 경계 | 충격 확인이 timeout 검사보다 먼저여서 늦은 충격 수용 | 같은 결함 | 600ms 만료를 먼저 검사 |
| 낙상 진동 | priority=3이어도 계단 연속 진동 분기가 먼저 반환, 낙상 OFF 구간 가려짐 | 낙상 패턴 먼저 유지 | 낙상 우선 유지, 브레이크는 별도 제어 |
| L5CX 재시도 | Wire 재시작, LPn 200/500ms 준비. 재시도 시각은 초기화 종료 후 | 버스 재시작 없이 100/100ms, 재시도 시각 초기화 전 | 버스 재시작/복구 클록/STOP, 200/500ms, 초기화 종료 기준 재시도. 실행 I2C timeout은 25ms 유지 |
| ToF 드라이버 | ST 오류 반환 수정. SparkFun은 외부 ^1.0.3 의존성 | ST 전송 mutex/오류 반환 + SparkFun 재할당 누적·부분 읽기 오류 수정, 고정 버전 포함 | 수정 드라이버 유지. 새 할당 누적 없는 begin 재시도 |
| LittleFS mount 실패 | 기존 내용과 관계없이 자동 format | 완전히 빈 파티션만 자동 format. 손상/기존 내용 보존 | 이전 보호 정책 유지. 오류 상태에서만 시리얼 명시적 초기화 허용 |
| 손상 파일 | 기존 profiles.bad 삭제 후 rename, rename 실패 검사 없음, 빈 상태 등록 허용 | 손상 파일을 그대로 두고 쓰기 차단 | 자동 삭제/덮어쓰기 차단 유지 |
| CFG/REG | CFG는 대기. 등록 중 새 CFG/연결 단절의 상태 정리 부족 | 새 CFG가 REG 취소, 연결 정리. 태그가 timeout 처리보다 먼저면 늦은 등록 가능 | BLE 세대 기록 + 카드 쓰기 직전 검증, 15초 마감 이전에만 등록 |
| 작은 MTU 알림 | min(len,payload)로 긴 메시지를 잘라 보냄 | 기본 모드 분할 금지, STREAM_ON에서 줄 단위 조립 | 명시적 스트림과 앱 교체 파일 유지. 기본 모드의 긴 메시지는 협상 대기 후 건너뜀 |
| BLE 이전 명령 | 단절 때 큐 삭제가 bleTick에 지연됨. main에서 blePop이 먼저여서 오래된 명령 실행 가능 | 세대별 큐 항목 검사 + 연결 확인 | 유지, 프로필 대기도 동일 세대 확인 |
| 오프라인 낙상 | 60초 지난 보류 알림은 조용히 버림 | 마지막 낙상 하나 RAM 보류 | 자동 TTL 폐기 없음. 전달/보호자 수신 ACK·전원 재시작 내구성은 구현하지 않음 |
| 앱 수신자 | 앱 교체 파일 없음 | RFID 화면이 주 수신자를 덮어쓰지 않는 add/removeListener, GATT 오류 처리, UTF-8 조립 | 세 파일 함께 제공 |
| 재접속 상태 | 현재 포고핀/계단/상부 상태 다시 전송 | 계단/상부는 동기화, 현재 포고핀 상태 누락 | 포고핀도 동기화. 안정 확인 중은 확인 중으로 표시 |
| LED | Q0 L5/Q1 MPU/Q2 계단/Q3 BLE/Q4 YOLO 정상/활성 | 동일 | 유지 |

센서 응답 기반 포고핀 상태는 실제 접점 위치를 직접 측정한 값이 아닙니다. 개별 모델/전원/I2C 고장과 물리 분리를 구분하는 정확도는 별도 실물 시험이 필요합니다.

## 재현한 클로드 제공본 동작

`python test/compare_claude_policy.py`는 원문에서 제공된 logic.h/feedback.cpp와 필요한 헤더를 추출해 PC에서 직접 컴파일합니다. 전체 클로드 PlatformIO 빌드는 아닙니다.

1. 타임아웃에 ECHO가 계속 HIGH인 입력 → NoObstacle 정상 결과.
2. 119 → 125 → 119cm → 120cm 미만이 연속이 아니지만 상부 감지.
3. 자유낙하 0ms → 충격 601ms → 정지 → 낙상 확정.
4. 낙상 패턴 700ms OFF 단계 + 계단 경고 → 중앙 모터 계속 ON.

해당 코드의 실제 결과를 assert로 확인했습니다. `docs/CLAUDE_POLICY_LOG.txt`가 출력 기록입니다. 통합본의 회귀 테스트는 각 결함을 수정한 동작을 검사합니다. `test/test_expansion.cpp`는 실제 expansion.cpp까지 컴파일해 첫 FAR 후 경고 거리 유지와 접점 흔들림 시 RFID 리셋 횟수를 확인합니다.

BLE 잘림·FS 자동 삭제·segmentTask의 블로킹 초기화 판단은 제공 파일의 실행 경로를 읽은 결과입니다. 클로드 코드 전체의 MCU/실물 실행 결과로 표현하지 않습니다.

## 제공 범위와 검증 한계

PlatformIO 프로젝트, 수정 ToF 드라이버, Android 교체 파일 3개, 테스트·로그·비교 자료를 제공합니다. `NAVCANE_v3_Claude_Review.md` 하나로 새 코드의 사용자 소스/수정 드라이버/앱/테스트를 검토할 수 있습니다. 실행에는 ZIP의 실제 폴더를 사용합니다.

ESP32 컴파일·링크·이미지 생성, 실제 C++ 구현의 PC 테스트, Android API33 클래스에 대한 Kotlin 컴파일, 메시지 조립 JVM 테스트를 수행했습니다. 전체 앱 Gradle/APK, 실제 Firebase/보호자 전달, 센서/서보/모터/휴대폰 시험과 전원 차단 내구 시험은 하지 않았습니다. 따라서 실물에서도 완벽하다고 보증할 수는 없습니다.
`````

## 파일: docs/KOREAN_LOGS.md

SHA-256: `9f8234b4e98f7957ac9ed8a581a3ba820e05374abaa49cd19fb406e13781fd08`

`````markdown
# 한국어 진단 로그 — v4.2

시리얼 모니터의 이벤트·센서 상태·초기화·저장소·진동 이유를 한국어로 표시합니다. 앱에 보내는 BLE 문자열, Pi UART, 핀맵과 감지 임계값은 유지합니다. 한국어 설명은 시리얼 전용이며 BLE로 추가 전송하지 않습니다.

## 무엇이 달라졌나

- `SENSOR:MPU:FAULT` 대신 가속도 사용 불가 안내와 상세 상태를 표시합니다.
- MPU의 공유 버스 대기 시간 초과, 읽기 응답 부족, 모델 ID 불일치, 설정 쓰기/검증 실패, 연속 읽기 실패를 구분합니다.
- 센서가 초기화됐어도 MPU는 120ms, 계단·장애물은 600ms부터 오래된 측정값을 사용하지 않습니다. 이때 ‘측정값 갱신 지연’이라고 표시합니다.
- MPU 사용 불가 때 가속도 표시를 -1로 바꿔 마지막 값이 계속 측정 중인 것처럼 보이지 않게 했습니다.
- L5CX가 LPn/버스 복구 중인지, 주소 응답 확인 후 펌웨어를 보내는 중인지, 초기화 실패 후 재시도를 기다리는지 표시합니다. 2.5초 무프레임 복구 정책은 유지합니다.
- 4CX는 미연결 또는 ID 읽기 실패를 구분해 안내합니다. 응답 실패만으로 부품이 고장이라고 단정하지 않습니다.
- 중앙 진동 패턴 시작 시 낙상·카드 처리·초록불 중 어떤 이유인지 표시합니다. 계단/좌우/상부 장애물 경고는 해당 이벤트와 상태에 표시합니다.
- 포고핀 메시지는 센서 응답에 따른 연결 추정임을 설명합니다. 최초 연결·통신 복구와 실제 물리 재결합은 같은 의미가 아닙니다.
- 슬롯 ID 판별 불가 때 GPIO36 외부 풀업 안내를 표시합니다. 슬롯 기능을 자동으로 끄거나 감지 임계값을 바꾸지 않았습니다.
- 처음 profiles.bin이 없는 상태는 ‘최초 등록 전에는 정상’이라고 안내합니다. 손상 저장소를 자동 삭제하지 않습니다.
- 최초 부팅에서 등록하지 않은 초음파 인터럽트를 해제하던 호출을 막았습니다. 리모컨 활성화/중단/재개에서는 인터럽트를 정상 등록·해제합니다.

## 표시 예시

아래는 형식을 설명하기 위한 예시이며 실제 실물 시험 로그가 아닙니다.

```text
[시작] NAVCANE v4.2 한국어 진단·고장 경고 무진동 버전. 센서 초기화는 별도 태스크에서 진행합니다.
[핀 안내] 중앙 진동=32, 왼쪽=14, 오른쪽=26, 버튼=33. GPIO33에 진동 드라이버를 연결하지 마세요.
[상태] 가속도=정상 (9.81 m/s²) | 계단=미연결 또는 모델 ID 읽기 실패 (주소 0x29) (-1.0cm, 위험=없음) | 장애물=정상 (왼쪽 45.0cm / 오른쪽 80.0cm)
[가속도 MPU6050] 측정 읽기 연속 2회 실패: I2C 응답 부족 또는 읽기 실패 (주소 0x68). 낙상 판정을 중지하고 자동 복구합니다.
[포고핀] 분절 센서 통신이 복구됐습니다. 실제 접촉을 직접 측정한 결과는 아닙니다.
[슬롯 오류] ID 전압으로 모듈 종류를 판별할 수 없습니다. 슬롯 미구성이라면 GPIO36의 외부 10kΩ 풀업이 필요합니다. 고장 경고 진동은 사용하지 않습니다.
[고장 경고] 진동 없이 로그와 앱 상태로 알립니다. 센서·슬롯 원인 로그를 확인하세요.
[낙상] 낙상으로 판정했습니다. 중앙 진동과 휴대폰 알림을 요청합니다.
```

## 원문 오류가 필요할 때

기본 `platformio.ini`의 `CORE_DEBUG_LEVEL=0`은 Arduino의 반복 영문 오류 출력을 줄입니다. 이것은 통신 오류를 해결하거나 숨겨 정상으로 판정하는 기능이 아닙니다. 사용자 코드의 상태/원인 로그와 사용 불가 판정은 계속 동작합니다.

원문 Arduino 오류를 함께 보려면 `platformio.ini`에서 `-DCORE_DEBUG_LEVEL=1`로 바꾸고 다시 빌드·업로드합니다. BLE 메시지 원문도 보려면 `include/config.h`의 `SERIAL_RAW_PROTOCOL=true`로 바꿉니다.

ESP32 ROM이 펌웨어 실행 전 출력하는 `ets`, `rst`, `boot`, `load`와 OS/PlatformIO의 안내, 일부 SDK의 오류는 영문이 남을 수 있습니다. 프로그램 실행 후 사용자 진단 로그를 한국어로 바꾼 것입니다.

## 현재 조립 상태의 해석

- 4CX 미연결: 계단 사용 불가가 예상됩니다. MPU는 독립적으로 복구합니다.
- 슬롯 회로 미구성: GPIO36이 떠서 UNKNOWN이나 다른 모듈로 오인식할 수 있습니다. 외부 10kΩ 풀업으로 빈 슬롯 전압을 정해야 합니다.
- 중앙 진동 드라이버 입력은 GPIO32, GPIO33은 버튼입니다.
- 한국어 진단 추가 자체가 기존 I2C 오류·낙상 오탐을 해결했다고 보장하지 않습니다. 배선 수정 후 새 원인 로그를 확인하세요.

## v4.2 고장 경고 진동 정책

고장 경고는 로그와 BLE 상태만 표시합니다. 센서 미연결·통신 실패·슬롯 오류 때문에 모터를 켜지 않으며, 진행 중인 다른 진동도 건드리지 않습니다. 낙상·계단·장애물·초록불·카드 처리 진동은 유지합니다.
`````

## 파일: docs/VALIDATION.md

SHA-256: `36c9ec199079ffd87e29367d5a2db3ace6979f394d0c3b6d6c8c0052fc6ad968`

`````markdown
# v4.2 고장 경고 무진동 검증

- 실제 feedback.cpp에서 반복 고장 경고가 세 진동 모터를 켜지 않는지 확인했습니다. 진행 중인 초록불·낙상 패턴도 중단·재시작하지 않습니다.
- 기존 main·센서 논리·프로필·확장모듈·BLE·드라이버 회귀 테스트 7개 모두 통과했습니다. `HOST_TEST_LOG_SILENT_FAULTS.txt` 참조.
- ESP32 빌드 통과: RAM 64,628/327,680바이트 (19.7%), Flash 1,380,085/1,966,080바이트 (70.2%). `BUILD_LOG_SILENT_FAULTS.txt` 참조.
- 고장 로그·BLE 상태·자동 복구, 낙상·계단·장애물·초록불·카드 진동과 핀맵을 유지합니다. 앱 코드는 변경하지 않았습니다.
- 실물 모터·센서 검증은 수행하지 않았습니다. 아래는 이전 버전 검증 기록입니다.

# v4.1 한국어 진단 보완 검증

- 실제 main/프로필/확장모듈/BLE/진동 코드와 드라이버 전송·논리의 PC 회귀 테스트 통과. `HOST_TEST_LOG_KOREAN.txt` 참조.
- 한국어 센서 원인/갱신 지연 표시를 확인하고, 한국어 시리얼 설명 추가 후에도 BLE에는 기존 `SENSOR:MPU:FAULT` 등 원문이 전달되는지 확인했습니다.
- ESP32 v4.1 빌드 통과: RAM 64,628/327,680바이트 (19.7%), Flash 1,380,025/1,966,080바이트 (70.2%). `BUILD_LOG_KOREAN.txt` 참조.
- Android 교체 파일은 변경하지 않았습니다. 아래 Android 검증은 v4에서 수행한 결과입니다.
- 실물 MCU·센서·모터·휴대폰으로 새 한국어 로그를 확인하지는 않았습니다. 한국어 진단 추가가 물리 I2C 오류나 낙상 오탐 해결을 보장하지 않습니다.

## 아래는 v4의 기존 검증 기록

# 검증 결과와 실물 확인 순서

## 완료한 검증

2026-10-02, 제공 ZIP에 들어가는 코드 기준입니다.

| 대상 | 결과 | 범위 |
|---|---|---|
| ESP32 펌웨어 | PASS | PlatformIO esp32dev, Espressif32 6.9.0, Arduino-ESP32 2.0.17 컴파일·링크·firmware.bin 생성 |
| 핀/판정 로직 | PASS | 핀 중복, ID 범위, 일시 출력 차단과 분리 확정, 재개/활성화, 타이머 wrap, 포고핀 3초+5회, 계단 히스테리시스·무타깃, 초음파 먼 응답/HIGH 고정, 이동 평균, LED 매핑, 낙상 시퀀스, 599/600/601ms 충격 경계, 상부 연속 판정 중간 거리 |
| 실제 main.cpp 판단/상태 | PASS | 확장모듈의 진동 재시작 없음, 기본 센서 새 고장 경고, 599/600ms 거리 신선도, 한글 계단 감지·해제와 GET_STATUS 동기화 |
| 실제 ObstacleRecovery | PASS | 실제 태스크에서 사용하는 상태 기계의 부팅, 실패 종료 후 5초 backoff, 600ms 간격에서 측정 유지, 2.5초 재초기화 즉시 예약, 새 프레임에 따른 watchdog 갱신, millis wrap |
| 실제 ToF 전송 함수 | PASS | 실제 수정 함수를 fake Wire와 컴파일. 긴 전송, NACK, 부분 읽기/쓰기, mutex 해제·락 획득 실패 반환 |
| 실제 profiles.cpp | PASS | CFG 미적용, REG/NEW/UPDATE/APPLY, 범위/형식 오류, timeout, 태그 직전 15초 경계·wrap, 세션 정리·빠른 재연결/단절 후 저장 차단, 부분 쓰기·CRC 검증 실패·rename 실패 시 이전 파일 유지, 손상 파일과 기존 파티션 보존, 명시적 초기화 |
| 실제 expansion.cpp | PASS | 첫 해제 후보에서 경고 거리 유지, 오류에 의한 연속 확인 리셋, 정상 응답 후 600ms 신선도, 먼 응답 복구, 공유 핀 high-Z, 순간 ID 흔들림 후 RC522 무리셋 재개 |
| 실제 ble_link.cpp | PASS | 첨부 앱의 CFG/REG 형태, 한 notify 한글, MTU 23/185, STREAM_ON 조각 전송, 프레임 중 낙상 발생 시 메시지 보존, 낙상 우선·5분 만료 직전/경계, 오프라인/CCCD OFF 만료, 새 사건 보관시간 갱신, 타이머 wrap, 프레임 전송 중 만료, 이전 세션 명령 폐기, 과도한 입력 |
| 실제 feedback.cpp | PASS | 상부 좌우 동시 진동, 계단 경고, 낙상 패턴 우선, 패턴 중 브레이크 유지, 패턴 완료 뒤 계단 경고 복귀 |
| Android Kotlin 파일 3개 | PASS | Kotlin 2.1.0으로 Android 13/API33 클래스에 대해 컴파일. Firebase/Fragment/기존 앱의 미제공 클래스는 API 형태의 스텁으로 연결 |
| 실제 앱 NavcaneLines / RfidConfig | PASS | JVM 실행. 한글 계단/포고핀 메시지의 1~20바이트 단위 UTF-8 조각, 여러 줄, 재연결 초기화, 길이 초과·NUL 폐기, RFID 범위/타임아웃 상수 |

Android 확인은 **전체 Gradle 프로젝트/APK 빌드가 아닙니다**. Activity/화면 XML/Manifest/실제 Firebase 의존성/보호자 전달 코드가 제공되지 않아 해당 부분의 연결은 확인하지 못했습니다. 실행한 Kotlin JVM 테스트와 C++ 테스트는 하드웨어/앱 스택 대체 객체를 사용합니다.

추가 비교: 클로드의 제공 logic.h/feedback.cpp를 PC에서 직접 컴파일해 HIGH 고정의 정상 분류, 비연속 상부 샘플의 감지, 601ms 충격 수용, 계단 경고에 의한 낙상 OFF 구간 가림을 재현했습니다. 클로드 프로젝트 전체의 ESP32 빌드는 수행하지 않았습니다. `CLAUDE_POLICY_LOG.txt`는 이 재현 결과입니다.

최종 ESP32 빌드는 별도 .pio 폴더에서 새로 컴파일했습니다. 정적 RAM 64,644/327,680바이트(19.7%), Flash 1,388,349/1,966,080바이트(70.6%)입니다.

빌드 크기는 `BUILD_LOG.txt` 마지막 부분에 기록했습니다. RAM 표시는 정적 사용량이며, 실행 중 BLE·태스크·드라이버는 힙을 추가로 사용합니다.

## 실물에서 확인할 순서

1. 현재 확정 센서가 VL53L4CX인지, GPIO와 XSHUT 유지 배선이 README와 맞는지 확인합니다. 서보 기구의 0°/20° 방향도 확인합니다.
2. 센서 연결 후 약 4초 안정 확인을 거쳐 MPU/계단 READY가 되는지 확인합니다. 정지한 MPU 가속도는 약 9.8m/s²입니다.
3. 센서에서 바닥까지 실제 사선 거리를 측정해 계단 임계값을 보정합니다. 정상 바닥, 임계값 근처, 먼 바닥, 무타깃, 유효하지 않은 타깃, I2C 단절, 프레임 정지를 나눠 기록합니다.
4. 계단 ToF만 응답이 없거나 재초기화 중일 때 정상 MPU의 새 샘플이 계속 오는지 확인합니다. MPU와 계단은 같은 물리 버스를 공유하므로 버스 자체 고착은 별도 조건입니다.
5. 공통 포고핀 분리/재결합과 한 센서 고장을 각각 시험합니다. 한글 메시지 횟수, 독립 복구, `MPU_MAX_GAP_MS`를 확인합니다. 이 누적 최대 간격에는 실제 MPU 분리 시간도 포함될 수 있으므로 MPU가 계속 연결된 시험과 구분합니다.
6. 슬롯을 가볍게 흔들어 순간 ID 변동 시 분리 로그/RC522 리셋이 반복되지 않는지, 지속 분리 시 출력이 중지되는지 확인합니다. ID 전압 허용 범위는 실제 저항·접점·ADC 조건에 맞춥니다.
7. 좌우 장애물과 상부 장애물을 각각 시험합니다. 장애물 프레임이 600ms 지나면 거리값을 쓰지 않는지, 2.5초 무프레임 후 추가 5초 대기 없이 초기화를 시작하는지, 초기화 실패 후에만 5초 대기하는지 확인합니다. 좌우 방향, 상부 120/130cm 경계, 양쪽 진동, 앱 한 줄 이벤트를 확인합니다.
8. HC-SR04P를 근거리/빈 공간/흡음 표면에서 확인합니다. ECHO의 실제 펄스 종료 시간과 LOW/HIGH 고정 조건을 기록합니다. 정상 범위 밖 응답에서 고장 진동이 반복되지 않아야 합니다.
9. 카드 A 적용 → 카드 B용 CFG 전송만 함 → A 설정 유지 → REG+B 태그 → B 설정 적용을 확인합니다. 재부팅 후 카드 재태그, 신규/갱신, UID 4/7/10바이트, 저장 중 전원 차단 후 이전 자료 보존을 확인합니다.
10. 앱 MTU 협상·CCCD 실패·재연결을 시험합니다. 기본 MTU 23에서도 교체 앱의 STREAM_ON으로 한글과 RFID 응답이 완전한 줄로 들어오는지 확인합니다.
11. RFID 화면을 열어 둔 상태에서도 기존 낙상/보호자 담당 수신자가 FALL_DETECTED를 받는지 확인합니다. BLE 단절 중 낙상 후 5분 미만/5분 경계/이후 재연결을 나눠 확인합니다. 오프라인·알림 OFF에서도 만료하고 새 낙상이 시간을 갱신해야 합니다. 실제 보호자 전달·재시도는 기존 앱 담당 코드에서 확인해야 합니다.
12. Q0 L5CX/Q1 MPU/Q2 L4CX/Q3 BLE/Q4 YOLO, Pi BTN/YOLO_ON/OFF/GREEN, 실제 리모컨 버튼 접점 전달을 확인합니다.

MPU는 15ms 목표로 예약합니다. OS 스케줄링, 물리 버스 고장과 I2C timeout, 플래시 작업 등에 따른 실제 간격은 실물에서 측정해야 합니다. LED/PC817에는 동작 결과 피드백 선이 없으므로 실제 LED 단선이나 리모컨 접점 전달 성공까지 자동 진단하지 않습니다.
`````

## 파일: docs/ANDROID_TEST_LOG.txt

SHA-256: `df22e23aab92b8cde92372f3bf810c53b9338c37af950a7f0d6ee49f7c0a785c`

`````text

Kotlin API33 compilation succeeded (exit 0). Missing app/Firebase/Fragment classes use test stubs.
Android JVM: Korean stair/pogo UTF8 chunks 1..20, multiple lines, reset, overflow, NUL and RFID limits PASS
`````

## 파일: docs/BUILD_LOG.txt

SHA-256: `e902fa294c224b48b9ba32dda0ea3261e0af1e12bd33781a0a99bc2754e4183f`

`````text
Processing esp32dev (platform: espressif32@6.9.0; board: esp32dev; framework: arduino)
--------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32dev.html
PLATFORM: Espressif 32 (6.9.0) > Espressif ESP32 Dev Module
HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
DEBUG: Current (cmsis-dap) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - framework-arduinoespressif32 @ 3.20017.241212+sha.dcc1105b 
 - tool-esptoolpy @ 1.40501.0 (4.5.1) 
 - toolchain-xtensa-esp32 @ 8.4.0+2021r2-patch5
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 36 compatible libraries
Scanning dependencies...
Dependency Graph
|-- MFRC522 @ 1.4.11
|-- ESP32 BLE Arduino @ 2.0.0
|-- SPI @ 2.0.0
|-- LittleFS @ 2.0.0
|-- SparkFun VL53L5CX Arduino Library @ 1.0.3
|-- Wire @ 2.0.0
|-- STM32duino VL53L4CX @ 1.1.0
Building in release mode
Compiling .pio/build/esp32dev/src/ble_link.cpp.o
Compiling .pio/build/esp32dev/src/expansion.cpp.o
Compiling .pio/build/esp32dev/src/feedback.cpp.o
Compiling .pio/build/esp32dev/src/main.cpp.o
Compiling .pio/build/esp32dev/src/profiles.cpp.o
Compiling .pio/build/esp32dev/src/sensors.cpp.o
Building .pio/build/esp32dev/bootloader.bin
Generating partitions .pio/build/esp32dev/partitions.bin
esptool.py v4.5.1
Creating esp32 image...
Merged 1 ELF section
Successfully created esp32 image.
Compiling .pio/build/esp32dev/liba96/SPI/SPI.cpp.o
Compiling .pio/build/esp32dev/lib977/MFRC522/MFRC522.cpp.o
Compiling .pio/build/esp32dev/lib977/MFRC522/MFRC522Extended.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLE2902.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLE2904.cpp.o
Archiving .pio/build/esp32dev/liba96/libSPI.a
Indexing .pio/build/esp32dev/liba96/libSPI.a
Compiling .pio/build/esp32dev/lib079/BLE/BLEAddress.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEAdvertisedDevice.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEAdvertising.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEBeacon.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLECharacteristic.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLECharacteristicMap.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEClient.cpp.o
Archiving .pio/build/esp32dev/lib977/libMFRC522.a
Indexing .pio/build/esp32dev/lib977/libMFRC522.a
Compiling .pio/build/esp32dev/lib079/BLE/BLEDescriptor.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEDescriptorMap.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEDevice.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEEddystoneTLM.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEEddystoneURL.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEExceptions.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEHIDDevice.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLERemoteCharacteristic.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLERemoteDescriptor.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLERemoteService.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEScan.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLESecurity.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEServer.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEService.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEServiceMap.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEUUID.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEUtils.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEValue.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/FreeRTOS.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/GeneralUtils.cpp.o
Compiling .pio/build/esp32dev/lib790/FS/FS.cpp.o
Compiling .pio/build/esp32dev/lib790/FS/vfs_api.cpp.o
Compiling .pio/build/esp32dev/lib241/LittleFS/LittleFS.cpp.o
Compiling .pio/build/esp32dev/lib767/Wire/Wire.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/SparkFun_VL53L5CX_IO.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/SparkFun_VL53L5CX_Library.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/platform.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/vl53l5cx_api.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/vl53l5cx_plugin_detection_thresholds.cpp.o
Archiving .pio/build/esp32dev/lib241/libLittleFS.a
Archiving .pio/build/esp32dev/lib079/libBLE.a
Indexing .pio/build/esp32dev/lib241/libLittleFS.a
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/vl53l5cx_plugin_motion_indicator.cpp.o
Indexing .pio/build/esp32dev/lib079/libBLE.a
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/vl53l5cx_plugin_xtalk.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api.cpp.o
Archiving .pio/build/esp32dev/lib767/libWire.a
Archiving .pio/build/esp32dev/lib790/libFS.a
Indexing .pio/build/esp32dev/lib767/libWire.a
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api_calibration.cpp.o
Indexing .pio/build/esp32dev/lib790/libFS.a
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api_core.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api_debug.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api_preset_modes.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_class.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_core.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_core_support.cpp.o
Archiving .pio/build/esp32dev/lib682/libSparkFun_VL53L5CX.a
Indexing .pio/build/esp32dev/lib682/libSparkFun_VL53L5CX.a
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_dmax.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_algos_gen3.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_algos_gen4.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_char.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_core.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_funcs.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_nvm.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_platform_ipp.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_register_funcs.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_sigma_estimate.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_silicon_core.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_wait.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_xtalk.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/Esp.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/FirmwareMSC.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/FunctionalInterrupt.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/HWCDC.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/HardwareSerial.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/IPAddress.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/IPv6Address.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/MD5Builder.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/Print.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/Stream.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/StreamString.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/Tone.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/USB.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/USBCDC.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/USBMSC.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/WMath.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/WString.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/base64.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/cbuf.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-adc.c.o
Archiving .pio/build/esp32dev/lib956/libVL53L4CX.a
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-bt.c.o
Indexing .pio/build/esp32dev/lib956/libVL53L4CX.a
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-cpu.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-dac.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-gpio.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-i2c-slave.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-i2c.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-ledc.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-matrix.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-misc.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-psram.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-rgb-led.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-rmt.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-sigmadelta.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-spi.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-time.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-timer.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-tinyusb.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-touch.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-uart.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/firmware_msc_fat.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/libb64/cdecode.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/libb64/cencode.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/main.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/stdlib_noniso.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/wiring_pulse.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/wiring_shift.c.o
Archiving .pio/build/esp32dev/libFrameworkArduino.a
Indexing .pio/build/esp32dev/libFrameworkArduino.a
Linking .pio/build/esp32dev/firmware.elf
Retrieving maximum program size .pio/build/esp32dev/firmware.elf
Checking size .pio/build/esp32dev/firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [==        ]  19.7% (used 64644 bytes from 327680 bytes)
Flash: [=======   ]  70.6% (used 1388349 bytes from 1966080 bytes)
Building .pio/build/esp32dev/firmware.bin with action:
  "$PYTHONEXE" "$OBJCOPY" --chip esp32 elf2image --flash_mode ${__get_board_flash_mode(__env__)} --flash_freq ${__get_board_f_image(__env__)} --flash_size 4MB --elf-sha256-offset 0xb0 -o $TARGET $SOURCES
Building .pio/build/esp32dev/firmware.bin
esptool.py v4.5.1
Creating esp32 image...
Merged 27 ELF sections
Successfully created esp32 image.
========================= [SUCCESS] Took 20.96 seconds =========================
`````

## 파일: docs/BUILD_LOG_KOREAN.txt

SHA-256: `dd755f927f29691ce36a7044316dd3bc31ff6c89fa195ab667bc6fff643f2470`

`````text
NAVCANE v4.1 한국어 진단 보완 / PlatformIO esp32dev 빌드 결과
명령: pio_env/bin/pio run -d NAVCANE_Integrated_v4
아래는 빌드 도구 설치 완료 전후부터 실제 컴파일·링크·용량 확인까지의 출력입니다.

Downloading 0% 10% 20% 30% 40% 50% 60% 70% 80% 90% 100%
Unpacking 0% 10% 20% 30% 40% 50% 60% 70% 80% 90% 100%
Tool Manager: tool-esptoolpy@1.40501.0 has been installed!
Tool Manager: Installing platformio/tool-scons @ ~4.41101.0
Downloading 0% 10%
Tool Manager: Warning! Package Mirror: The checksum '5b131ca14aa96311d3432b0062c7443d3b0e6346ec279bd376a75f0a76bcd5d7' of the downloaded file 'tool-scons-4.41101.0.tar.gz' does not match to the remote 'e405de1fcd3307fa80d624f0290c7cd30fbb47f67e135a9f56d7e46b6b895311'
Tool Manager: Looking for another mirror...
Downloading 0% 10% 20% 30% 40% 50% 60% 70% 80% 90% 100%
Unpacking 0% 10% 20% 30% 40% 50% 60% 70% 80% 90% 100%
Tool Manager: tool-scons@4.41101.0 has been installed!
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32dev.html
PLATFORM: Espressif 32 (6.9.0) > Espressif ESP32 Dev Module
HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
DEBUG: Current (cmsis-dap) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - framework-arduinoespressif32 @ 3.20017.241212+sha.dcc1105b 
 - tool-esptoolpy @ 1.40501.0 (4.5.1) 
 - toolchain-xtensa-esp32 @ 8.4.0+2021r2-patch5
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 36 compatible libraries
Scanning dependencies...
Dependency Graph
|-- MFRC522 @ 1.4.11
|-- ESP32 BLE Arduino @ 2.0.0
|-- SPI @ 2.0.0
|-- LittleFS @ 2.0.0
|-- SparkFun VL53L5CX Arduino Library @ 1.0.3
|-- Wire @ 2.0.0
|-- STM32duino VL53L4CX @ 1.1.0
Building in release mode
Compiling .pio/build/esp32dev/src/ble_link.cpp.o
Compiling .pio/build/esp32dev/src/expansion.cpp.o
Compiling .pio/build/esp32dev/src/feedback.cpp.o
Compiling .pio/build/esp32dev/src/main.cpp.o
Compiling .pio/build/esp32dev/src/profiles.cpp.o
Compiling .pio/build/esp32dev/src/sensors.cpp.o
Building .pio/build/esp32dev/bootloader.bin
Generating partitions .pio/build/esp32dev/partitions.bin
Compiling .pio/build/esp32dev/liba96/SPI/SPI.cpp.o
esptool.py v4.5.1
Creating esp32 image...
Merged 1 ELF section
Successfully created esp32 image.
Compiling .pio/build/esp32dev/lib977/MFRC522/MFRC522.cpp.o
Compiling .pio/build/esp32dev/lib977/MFRC522/MFRC522Extended.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLE2902.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLE2904.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEAddress.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEAdvertisedDevice.cpp.o
Archiving .pio/build/esp32dev/liba96/libSPI.a
Indexing .pio/build/esp32dev/liba96/libSPI.a
Compiling .pio/build/esp32dev/lib079/BLE/BLEAdvertising.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEBeacon.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLECharacteristic.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLECharacteristicMap.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEClient.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEDescriptor.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEDescriptorMap.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEDevice.cpp.o
Archiving .pio/build/esp32dev/lib977/libMFRC522.a
Indexing .pio/build/esp32dev/lib977/libMFRC522.a
Compiling .pio/build/esp32dev/lib079/BLE/BLEEddystoneTLM.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEEddystoneURL.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEExceptions.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEHIDDevice.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLERemoteCharacteristic.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLERemoteDescriptor.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLERemoteService.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEScan.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLESecurity.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEServer.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEService.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEServiceMap.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEUUID.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEUtils.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/BLEValue.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/FreeRTOS.cpp.o
Compiling .pio/build/esp32dev/lib079/BLE/GeneralUtils.cpp.o
Compiling .pio/build/esp32dev/lib790/FS/FS.cpp.o
Compiling .pio/build/esp32dev/lib790/FS/vfs_api.cpp.o
Compiling .pio/build/esp32dev/lib241/LittleFS/LittleFS.cpp.o
Compiling .pio/build/esp32dev/lib767/Wire/Wire.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/SparkFun_VL53L5CX_IO.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/SparkFun_VL53L5CX_Library.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/platform.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/vl53l5cx_api.cpp.o
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/vl53l5cx_plugin_detection_thresholds.cpp.o
Archiving .pio/build/esp32dev/lib079/libBLE.a
Indexing .pio/build/esp32dev/lib079/libBLE.a
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/vl53l5cx_plugin_motion_indicator.cpp.o
Archiving .pio/build/esp32dev/lib241/libLittleFS.a
Indexing .pio/build/esp32dev/lib241/libLittleFS.a
Compiling .pio/build/esp32dev/lib682/SparkFun_VL53L5CX/vl53l5cx_plugin_xtalk.cpp.o
Archiving .pio/build/esp32dev/lib790/libFS.a
Indexing .pio/build/esp32dev/lib790/libFS.a
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api.cpp.o
Archiving .pio/build/esp32dev/lib767/libWire.a
Indexing .pio/build/esp32dev/lib767/libWire.a
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api_calibration.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api_core.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api_debug.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_api_preset_modes.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_class.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_core.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_core_support.cpp.o
Archiving .pio/build/esp32dev/lib682/libSparkFun_VL53L5CX.a
Indexing .pio/build/esp32dev/lib682/libSparkFun_VL53L5CX.a
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_dmax.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_algos_gen3.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_algos_gen4.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_char.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_core.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_hist_funcs.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_nvm.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_platform_ipp.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_register_funcs.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_sigma_estimate.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_silicon_core.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_wait.cpp.o
Compiling .pio/build/esp32dev/lib956/VL53L4CX/vl53l4cx_xtalk.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/Esp.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/FirmwareMSC.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/FunctionalInterrupt.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/HWCDC.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/HardwareSerial.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/IPAddress.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/IPv6Address.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/MD5Builder.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/Print.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/Stream.cpp.o
Archiving .pio/build/esp32dev/lib956/libVL53L4CX.a
Indexing .pio/build/esp32dev/lib956/libVL53L4CX.a
Compiling .pio/build/esp32dev/FrameworkArduino/StreamString.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/Tone.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/USB.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/USBCDC.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/USBMSC.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/WMath.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/WString.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/base64.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/cbuf.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-adc.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-bt.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-cpu.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-dac.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-gpio.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-i2c-slave.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-i2c.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-ledc.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-matrix.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-misc.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-psram.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-rgb-led.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-rmt.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-sigmadelta.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-spi.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-time.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-timer.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-tinyusb.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-touch.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/esp32-hal-uart.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/firmware_msc_fat.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/libb64/cdecode.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/libb64/cencode.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/main.cpp.o
Compiling .pio/build/esp32dev/FrameworkArduino/stdlib_noniso.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/wiring_pulse.c.o
Compiling .pio/build/esp32dev/FrameworkArduino/wiring_shift.c.o
Archiving .pio/build/esp32dev/libFrameworkArduino.a
Indexing .pio/build/esp32dev/libFrameworkArduino.a
Linking .pio/build/esp32dev/firmware.elf
Retrieving maximum program size .pio/build/esp32dev/firmware.elf
Checking size .pio/build/esp32dev/firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [==        ]  19.7% (used 64628 bytes from 327680 bytes)
Flash: [=======   ]  70.2% (used 1380025 bytes from 1966080 bytes)
Building .pio/build/esp32dev/firmware.bin with action:
  "$PYTHONEXE" "$OBJCOPY" --chip esp32 elf2image --flash_mode ${__get_board_flash_mode(__env__)} --flash_freq ${__get_board_f_image(__env__)} --flash_size 4MB --elf-sha256-offset 0xb0 -o $TARGET $SOURCES
Building .pio/build/esp32dev/firmware.bin
esptool.py v4.5.1
Creating esp32 image...
Merged 27 ELF sections
Successfully created esp32 image.
======================== [SUCCESS] Took 491.85 seconds ========================

`````

## 파일: docs/BUILD_LOG_SILENT_FAULTS.txt

SHA-256: `1614579dcddbc07d893e91f54af2f1ebd4011bab8ed49469a64396202cb91223`

`````text
Processing esp32dev (platform: espressif32@6.9.0; board: esp32dev; framework: arduino)
--------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32dev.html
PLATFORM: Espressif 32 (6.9.0) > Espressif ESP32 Dev Module
HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
DEBUG: Current (cmsis-dap) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - framework-arduinoespressif32 @ 3.20017.241212+sha.dcc1105b 
 - tool-esptoolpy @ 1.40501.0 (4.5.1) 
 - toolchain-xtensa-esp32 @ 8.4.0+2021r2-patch5
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 36 compatible libraries
Scanning dependencies...
Dependency Graph
|-- MFRC522 @ 1.4.11
|-- ESP32 BLE Arduino @ 2.0.0
|-- SPI @ 2.0.0
|-- LittleFS @ 2.0.0
|-- SparkFun VL53L5CX Arduino Library @ 1.0.3
|-- Wire @ 2.0.0
|-- STM32duino VL53L4CX @ 1.1.0
Building in release mode
Compiling .pio/build/esp32dev/src/ble_link.cpp.o
Compiling .pio/build/esp32dev/src/feedback.cpp.o
Compiling .pio/build/esp32dev/src/main.cpp.o
Linking .pio/build/esp32dev/firmware.elf
Retrieving maximum program size .pio/build/esp32dev/firmware.elf
Checking size .pio/build/esp32dev/firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [==        ]  19.7% (used 64628 bytes from 327680 bytes)
Flash: [=======   ]  70.2% (used 1380085 bytes from 1966080 bytes)
Building .pio/build/esp32dev/firmware.bin with action:
  "$PYTHONEXE" "$OBJCOPY" --chip esp32 elf2image --flash_mode ${__get_board_flash_mode(__env__)} --flash_freq ${__get_board_f_image(__env__)} --flash_size 4MB --elf-sha256-offset 0xb0 -o $TARGET $SOURCES
Building .pio/build/esp32dev/firmware.bin
esptool.py v4.5.1
Creating esp32 image...
Merged 27 ELF sections
Successfully created esp32 image.
========================= [SUCCESS] Took 8.14 seconds =========================
`````

## 파일: docs/CLAUDE_POLICY_LOG.txt

SHA-256: `12ebe15b5240d1c450895b9dc4db5e7b17b9d6aabe60d575bc86b8f7f4fa0702`

`````text
Claude: HIGH remaining at timeout is classified as normal NoObstacle
Claude: 119/125/119cm triggers despite nonconsecutive below-120 samples
Claude: impact at 601ms after freefall is accepted as a fall sequence
Claude: stair alert keeps central motor ON during the fall pattern OFF interval
`````

## 파일: docs/HOST_TEST_LOG.txt

SHA-256: `beadd8494c7caedfb719715c4e4d3e3d23c29b3b7ef34ea24069ceb2996aed3e`

`````text
main: no duplicate expansion fault, base sensor warning, 599/600ms freshness, Korean stair events and status sync PASS
logic: obstacle recovery/backoff/wrap, ID, detach, debounce, stair and fall PASS
driver transport: long transfers, NACK, short reads/writes PASS
profiles: app CFG/REG/APPLY, errors, timeout, session cleanup, short write, CRC, rename failure, mount protection PASS
expansion: confirmed upper clearing, held warning distance, stale/fault release, far recovery, slot high-Z and reset-free resume PASS
BLE: framing, fall priority, 5min expiry/offline/CCCD/wrap/replacement, epochs and input limits PASS
feedback: upper both-sides, stair caution, fall priority, retained brake and pattern completion PASS
`````

## 파일: docs/HOST_TEST_LOG_KOREAN.txt

SHA-256: `beadd8494c7caedfb719715c4e4d3e3d23c29b3b7ef34ea24069ceb2996aed3e`

`````text
main: no duplicate expansion fault, base sensor warning, 599/600ms freshness, Korean stair events and status sync PASS
logic: obstacle recovery/backoff/wrap, ID, detach, debounce, stair and fall PASS
driver transport: long transfers, NACK, short reads/writes PASS
profiles: app CFG/REG/APPLY, errors, timeout, session cleanup, short write, CRC, rename failure, mount protection PASS
expansion: confirmed upper clearing, held warning distance, stale/fault release, far recovery, slot high-Z and reset-free resume PASS
BLE: framing, fall priority, 5min expiry/offline/CCCD/wrap/replacement, epochs and input limits PASS
feedback: upper both-sides, stair caution, fall priority, retained brake and pattern completion PASS
`````

## 파일: docs/HOST_TEST_LOG_SILENT_FAULTS.txt

SHA-256: `391112c13b28c0db6269e6746e53b60e14ab7ba88b8e096b21331af0b860b612`

`````text
main: no duplicate expansion fault, base sensor warning, 599/600ms freshness, Korean stair events and status sync PASS
logic: obstacle recovery/backoff/wrap, ID, detach, debounce, stair and fall PASS
driver transport: long transfers, NACK, short reads/writes PASS
profiles: app CFG/REG/APPLY, errors, timeout, session cleanup, short write, CRC, rename failure, mount protection PASS
expansion: confirmed upper clearing, held warning distance, stale/fault release, far recovery, slot high-Z and reset-free resume PASS
BLE: framing, fall priority, 5min expiry/offline/CCCD/wrap/replacement, epochs and input limits PASS
feedback: silent faults, uninterrupted green/fall, upper both-sides, stair caution, retained brake and pattern completion PASS
`````

## 파일: test/NavcaneProtocolTest.kt

SHA-256: `aae735359d4d4f5ec37d3ff3f2db55df025ed5d19301f49b0c88459a0808f279`

`````kotlin
package com.example.whitecaneguardian

fun main() {
    val expected = listOf("포고핀 분리상태", "계단 감지", "STAIR_CLEARED", "FALL_DETECTED", "REG,OK,01020304,NEW,ALICE")
    val bytes = (expected.joinToString("\n") + "\n").toByteArray(Charsets.UTF_8)
    for (chunkSize in 1..20) {
        val parser = NavcaneLines()
        val actual = mutableListOf<String>()
        var offset = 0
        while (offset < bytes.size) {
            val end = (offset + chunkSize).coerceAtMost(bytes.size)
            actual += parser.push(bytes.copyOfRange(offset, end)); offset = end
        }
        check(actual == expected) { "UTF8 framing failed at $chunkSize bytes" }
    }
    val parser = NavcaneLines()
    check(parser.push("PARTIAL".toByteArray()).isEmpty())
    parser.clear()
    check(parser.push("ACK,CFG\r\nPONG\n".toByteArray()) == listOf("ACK,CFG", "PONG"))
    check(parser.push(("X".repeat(97) + "\nFALL_DETECTED\n").toByteArray()) == listOf("FALL_DETECTED"))
    check(parser.droppedLines == 1)
    check(parser.push("BAD\u0000LINE\nREG,TIMEOUT\n".toByteArray()) == listOf("REG,TIMEOUT"))
    check(parser.droppedLines == 2)
    check(RfidConfig.NAME_REGEX.matches("USER123"))
    check(!RfidConfig.NAME_REGEX.matches("A B"))
    check(RfidConfig.OBSTACLE_MIN_CM == 20 && RfidConfig.OBSTACLE_MAX_CM == 200)
    check(RfidConfig.HEIGHT_CODE_MIN == 14 && RfidConfig.HEIGHT_CODE_MAX == 19)
    check(RfidConfig.REG_WAIT_SEC == 15 && RfidConfig.ACK_TIMEOUT_MS == 5000L)
    println("Android JVM: UTF8 chunks 1..20, multiple lines, reset, overflow, NUL and shared RFID limits PASS")
}
`````

## 파일: test/android_compile_stubs/App.kt

SHA-256: `fc45ed4462a74642468820c4d628bd7d327869cc8cd82ebd17bdab74dd64d5f5`

`````kotlin
package com.example.whitecaneguardian
import android.content.Context
object BleLog { fun add(entry: BleLogEntry) {} }
data class BleLogEntry(val time:Long,val type:String,val detail:String)
object VoiceTrigger { var calls=0; fun fire() { ++calls } }
object BleHolder { fun get(context:Context):BleManager=BleManager(context) }
object R {
 object layout { const val fragment_rfid=1 }
 object id { const val etRfidName=1; const val etRfidObstacle=2; const val tvRfidHeight=3; const val rgVibration=4; const val btnRfidSend=5; const val btnRfidRegister=6; const val tvRfidStatus=7; const val rbVibStrong=8 }
}
`````

## 파일: test/android_compile_stubs/Auth.kt

SHA-256: `27d8db3ce3a9f06378209dd11148b23ec516ebbe55380540124b9a4fb3dc445d`

`````kotlin
package com.google.firebase.auth
class FirebaseUser { val uid:String?="user" }
class FirebaseAuth {
 val currentUser:FirebaseUser?=FirebaseUser()
 companion object { fun getInstance():FirebaseAuth=FirebaseAuth() }
}
`````

## 파일: test/android_compile_stubs/Firestore.kt

SHA-256: `e6e96f2470c440798cfa6b888356ba91cbeb04680d50dc480d936e5f3d391247`

`````kotlin
package com.google.firebase.firestore
class DocumentSnapshot { fun getDouble(key:String):Double?=170.0 }
class Task {
 fun addOnSuccessListener(listener:(DocumentSnapshot)->Unit):Task=this
 fun addOnFailureListener(listener:(Exception)->Unit):Task=this
}
class CollectionReference { fun document(path:String):DocumentReference=DocumentReference() }
class DocumentReference { fun get():Task=Task() }
class FirebaseFirestore {
 fun collection(path:String):CollectionReference=CollectionReference()
 companion object { fun getInstance():FirebaseFirestore=FirebaseFirestore() }
}
`````

## 파일: test/android_compile_stubs/Fragment.kt

SHA-256: `3cac48aa7c86e0a4e3890876c3519d035675ada05c6a9aad7bea579cc69c1857`

`````kotlin
package androidx.fragment.app
import android.os.Bundle
import android.view.View
import android.view.LayoutInflater
import android.view.ViewGroup
import android.content.Context
import android.app.Activity
open class Fragment {
 val isAdded=true
 val activity:Activity?=null
 fun requireContext():Context=throw UnsupportedOperationException()
 open fun onCreateView(inflater:LayoutInflater,container:ViewGroup?,savedInstanceState:Bundle?):View?=null
 open fun onViewCreated(view:View,savedInstanceState:Bundle?) {}
 open fun onDestroyView() {}
}
`````

## 파일: test/compare_claude_policy.py

SHA-256: `50896edf9cdfc3897c54ae178e85c91b9415f80ff5a093cf2acbcf586f5f6f18`

`````python
#!/usr/bin/env python3
"""Reproduce observed Claude policies; this is not an ESP32 build of that project."""
from pathlib import Path
import re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
original = (root/'reference/CLAUDE_ORIGINAL.md').read_text()
files = dict(re.findall(r'^## ([^\n]+)\n\n```[^\n]*\n(.*?)\n```', original, re.M | re.S))
harness = '#include <cassert>\n#include <cstdio>\n#include "logic.h"\n#include "feedback.cpp"\nfloat activeObstacleThresholdCM=50, activeStairThresholdCM=170;\nuint32_t activeVibDuty=255;\nchar activeProfileName[13]="DEFAULT";\nint main() {\n  float cm;\n  assert(nav::classifyEcho(false,0,true,true,cm)==nav::EchoResult::NoObstacle);\n  puts("Claude: HIGH remaining at timeout is classified as normal NoObstacle");\n  nav::UpperObstacle upper;\n  upper.sample(119); upper.sample(125); upper.sample(119); assert(upper.active);\n  puts("Claude: 119/125/119cm triggers despite nonconsecutive below-120 samples");\n  nav::FallDetector fall;\n  fall.sample(2,0); fall.sample(20,601); fall.sample(9.8,1101);\n  assert(fall.sample(9.8,2101));\n  puts("Claude: impact at 601ms after freefall is accepted as a fall sequence");\n  feedbackBegin(); fakeMillis=0; feedbackStart(Feedback::Fall);\n  fakeMillis=700; feedbackTick(true,-1,-1,false,-1);\n  assert(fakeDuty[cfg::VIB_CH_MAIN]==activeVibDuty);\n  puts("Claude: stair alert keeps central motor ON during the fall pattern OFF interval");\n}\n'
with tempfile.TemporaryDirectory() as directory:
    temp=Path(directory)
    for name in ('config.h','logic.h','feedback.cpp','feedback.h','profiles.h'):
        (temp/name).write_text(files['src/'+name])
    source=temp/'policy.cpp'; source.write_text(harness)
    binary=temp/'policy'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)
`````

## 파일: test/run_host_tests.py

SHA-256: `50f22161ac8ff1d4a7b4797c1c9da4de97a5519b7cf1b1aa761b58dd5b7a4cf4`

`````python
#!/usr/bin/env python3
"""Exercise production policy and the exact patched driver transport with fake Wire."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'main'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_main.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)

def transport(file, start, end):
    text = file.read_text()
    return text[text.index(start):text.index(end, text.index(start))]

st = transport(root/'lib/VL53L4CX/src/vl53l4cx_class.cpp',
               "extern \"C\" bool navSegmentLock();",
               'VL53L4CX_Error VL53L4CX::VL53L4CX_GetTickCount(')
sf = transport(root/'lib/SparkFun_VL53L5CX/src/SparkFun_VL53L5CX_IO.cpp',
               'uint8_t SparkFun_VL53L5CX_IO::writeMultipleBytes(',
               'uint8_t SparkFun_VL53L5CX_IO::readSingleByte(')
stub = r'''
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdio>
#define highByte(x) uint8_t((x)>>8)
#define lowByte(x) uint8_t(x)
static int locks=0, unlocks=0, nesting=0;
static bool lockAllowed=true;
extern "C" bool navSegmentLock() { if(!lockAllowed) return false; ++locks; assert(nesting++==0); return true; }
extern "C" void navSegmentUnlock() { ++unlocks; assert(--nesting==0); }
using VL53L4CX_Error = int;
constexpr int VL53L4CX_ERROR_NONE=0, VL53L4CX_ERROR_CONTROL_INTERFACE=-1;
class FakeWire {
 public:
  uint8_t memory[65536] = {};
  std::vector<uint8_t> tx;
  uint16_t reg=0; int remaining=0;
  bool nack=false, shortRead=false, shortWrite=false;
  int ends=0, requests=0;
  void beginTransmission(uint8_t) { tx.clear(); }
  size_t write(uint8_t b) { tx.push_back(b); return 1; }
  size_t write(uint8_t *b,size_t n) { tx.insert(tx.end(),b,b+n); return shortWrite?n-1:n; }
  int endTransmission(bool=true) {
    ++ends; if(nack) return 4;
    assert(tx.size()>=2); reg=(uint16_t(tx[0])<<8)|tx[1];
    for(size_t i=2;i<tx.size();++i) memory[reg++]=tx[i]; return 0;
  }
  uint8_t requestFrom(uint8_t,uint8_t n) { ++requests; remaining=shortRead?n-1:n; return remaining; }
  int available() { return remaining; }
  int read() { if(!remaining) return -1; --remaining; return memory[reg++]; }
};
class VL53L4CX {
 public:
  FakeWire *dev_i2c;
  explicit VL53L4CX(FakeWire *w):dev_i2c(w){}
  int VL53L4CX_I2CWrite(uint8_t,uint16_t,uint8_t*,uint16_t);
  int VL53L4CX_I2CRead(uint8_t,uint16_t,uint8_t*,uint16_t);
};
class SparkFun_VL53L5CX_IO {
 public:
  FakeWire *_i2cPort; uint8_t _address=0x29; uint16_t wireMaxPacketSize=32;
  explicit SparkFun_VL53L5CX_IO(FakeWire *w):_i2cPort(w){}
  uint8_t writeMultipleBytes(uint16_t,uint8_t*,uint16_t);
  uint8_t readMultipleBytes(uint16_t,uint8_t*,uint16_t);
};
'''
checks = r'''
int main() {
  FakeWire wire; VL53L4CX st(&wire); SparkFun_VL53L5CX_IO sf(&wire);
  uint8_t source[99], out[99]; for(int i=0;i<99;++i) source[i]=i;
  assert(!st.VL53L4CX_I2CWrite(0x52,0x1234,source,99));
  assert(!st.VL53L4CX_I2CRead(0x52,0x1234,out,99));
  assert(!memcmp(source,out,99)); assert(wire.requests==4);
  wire.nack=true; int before=wire.ends;
  assert(st.VL53L4CX_I2CRead(0x52,0x1234,out,99)!=0);
  assert(wire.ends-before==1); // failed ACK MUST return, no infinite retry
  assert(st.VL53L4CX_I2CWrite(0x52,0x1234,source,99)!=0);
  wire.nack=false; wire.shortRead=true;
  assert(st.VL53L4CX_I2CRead(0x52,0x1234,out,99)!=0);
  assert(sf.readMultipleBytes(0x1234,out,20)!=0); // upstream returned false == success
  wire.shortRead=false; wire.shortWrite=true;
  assert(st.VL53L4CX_I2CWrite(0x52,0x1234,source,20)!=0);
  wire.shortWrite=false;
  assert(!sf.writeMultipleBytes(0x1234,source,99));
  assert(!sf.readMultipleBytes(0x1234,out,99)); assert(!memcmp(source,out,99));
  wire.nack=true; assert(sf.readMultipleBytes(0x1234,out,99)!=0);
  assert(sf.writeMultipleBytes(0x1234,source,99)!=0);
  assert(locks==unlocks && nesting==0);
  lockAllowed=false; int beforeLock=wire.ends;
  assert(st.VL53L4CX_I2CRead(0x52,0x1234,out,99)!=0);
  assert(wire.ends==beforeLock && locks==unlocks && nesting==0);
  puts("driver transport: long transfers, NACK, short reads/writes PASS");
}
'''
with tempfile.TemporaryDirectory() as temp:
    temp = Path(temp)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(root/'include'),
                    str(root/'test/test_logic.cpp'),'-o',str(temp/'logic')],check=True)
    subprocess.run([str(temp/'logic')],check=True,timeout=5)
    source = temp/'transport.cpp'; source.write_text(stub+st+sf+checks)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Wno-misleading-indentation',
                    str(source),'-o',str(temp/'transport')],check=True)
    subprocess.run([str(temp/'transport')],check=True,timeout=5)

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'profiles'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_profiles.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'expansion'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_expansion.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'ble'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_ble.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'feedback'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_feedback.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)
`````

## 파일: test/stubs/Arduino.h

SHA-256: `83ace5a2745716a7e13d52ed74f3cb24e78443bf9febefecdc175607a7aeaede`

`````cpp
#pragma once
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cctype>
template<typename T> T min(T a,T b) { return std::min(a,b); }
class String {
  std::string value;
 public:
  String() = default;
  String(const char *s):value(s ? s : ""){}
  String(const std::string &s):value(s){}
  String(int v):value(std::to_string(v)){}
  String(long v):value(std::to_string(v)){}
  String(unsigned int v):value(std::to_string(v)){}
  String(unsigned long v):value(std::to_string(v)){}
  String(float v,int digits=2) { char b[80]; snprintf(b,sizeof(b),"%.*f",digits,v); value=b; }
  unsigned int length() const { return value.size(); }
  const char *c_str() const { return value.c_str(); }
  bool startsWith(const char *s) const { return value.rfind(s,0)==0; }
  bool endsWith(const char *s) const { size_t n=strlen(s); return n<=value.size() && value.compare(value.size()-n,n,s)==0; }
  int indexOf(char c,int from=0) const { auto p=value.find(c,from); return p==std::string::npos?-1:int(p); }
  String substring(int from) const { return value.substr(from); }
  String substring(int from,int end) const { return value.substr(from,end-from); }
  long toInt() const { return std::stol(value); }
  char operator[](size_t i) const { return value[i]; }
  String &operator+=(char c) { value+=c; return *this; }
  String &operator+=(const String &s) { value+=s.value; return *this; }
  bool operator==(const String &s) const { return value==s.value; }
  bool operator!=(const String &s) const { return !(*this==s); }
  friend String operator+(const String &a,const String &b) { return a.value+b.value; }
  void trim() { auto a=value.find_first_not_of(" \r\n\t"); auto b=value.find_last_not_of(" \r\n\t"); value=a==std::string::npos?"":value.substr(a,b-a+1); }
  void toCharArray(char *out,size_t n) const { if(n) { std::strncpy(out,value.c_str(),n); out[n-1]=0; } }
};
inline uint32_t fakeMillis=0;
inline unsigned long millis() { return fakeMillis; }
struct FakeSerial {
  void begin(uint32_t,int=0,uint8_t=0,uint8_t=0) {}
  int available() { return 0; }
  int read() { return -1; }
  void print(const char *) {}
  void println(const String &) {}
  template<typename... Args> void printf(const char *,Args...) {}
};
inline FakeSerial Serial;
inline FakeSerial Serial2;
inline void delay(uint32_t) {}
constexpr int INPUT_PULLUP=4, SERIAL_8N1=0;
struct FakeESP { uint32_t getFreeHeap() { return 200000; } };
inline FakeESP ESP;
inline uint32_t fakeDuty[16]={};
inline void ledcWrite(uint8_t channel,uint32_t duty) { fakeDuty[channel]=duty; }
inline void ledcSetup(uint8_t,int,int) {}
inline void ledcAttachPin(uint8_t,uint8_t) {}
template<typename T> T constrain(T value,T lo,T hi) { return std::max(lo,std::min(value,hi)); }
inline long map(long v,long a,long b,long c,long d) { return (v-a)*(d-c)/(b-a)+c; }
using byte=uint8_t;
#define IRAM_ATTR
constexpr int LOW=0, HIGH=1, INPUT=0, OUTPUT=1, CHANGE=2, ADC_11db=3, MSBFIRST=1;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL(portMUX_TYPE *) {}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE *) {}
inline int fakePinModes[40]={}, fakePins[40]={};
inline uint16_t fakeAdcMv=3300;
inline void pinMode(uint8_t pin,int mode) { fakePinModes[pin]=mode; }
inline void digitalWrite(uint8_t pin,int value) { fakePins[pin]=value; }
inline int digitalRead(uint8_t pin) { return fakePins[pin]; }
inline uint32_t micros() { return fakeMillis*1000u; }
inline void delayMicroseconds(uint32_t) {}
inline int digitalPinToInterrupt(uint8_t pin) { return pin; }
inline void attachInterrupt(int,void (*)(),int) {}
inline void detachInterrupt(int) {}
inline void analogSetPinAttenuation(uint8_t,int) {}
inline uint16_t analogReadMilliVolts(uint8_t) { return fakeAdcMv; }
inline void shiftOut(uint8_t,uint8_t,int,uint8_t) {}
`````

## 파일: test/stubs/BLE2902.h

SHA-256: `030c5b5382d1d64e47ca0f024bbc274036291739e87cde2c1bce03bb6f6b72a6`

`````cpp
#pragma once
#include "BLEDevice.h"
`````

## 파일: test/stubs/BLEDevice.h

SHA-256: `9bb15360b892f492156e1f669f0d32875b91b21adff6094c2a1dc2c594349806`

`````cpp
#pragma once
#include "Arduino.h"
#include <deque>
#include <vector>
#include <cassert>
struct FakeQueue { size_t capacity,itemSize; std::deque<std::vector<uint8_t>> entries; };
using QueueHandle_t=FakeQueue *;
constexpr int pdTRUE=1;
inline QueueHandle_t xQueueCreate(size_t n,size_t size) { return new FakeQueue{n,size,{}}; }
inline int xQueueSend(QueueHandle_t q,const void *p,int) { if(q->entries.size()==q->capacity) return 0; const auto *b=static_cast<const uint8_t *>(p); q->entries.emplace_back(b,b+q->itemSize); return pdTRUE; }
inline int xQueueReceive(QueueHandle_t q,void *p,int) { if(q->entries.empty()) return 0; memcpy(p,q->entries.front().data(),q->itemSize); q->entries.pop_front(); return pdTRUE; }
inline void xQueueReset(QueueHandle_t q) { q->entries.clear(); }
struct esp_ble_gatts_cb_param_t { struct { uint16_t mtu=23; } mtu; };
class BLEServer;
class BLEServerCallbacks { public: virtual ~BLEServerCallbacks()=default; virtual void onConnect(BLEServer *){} virtual void onDisconnect(BLEServer *){} virtual void onMtuChanged(BLEServer *,esp_ble_gatts_cb_param_t *){} };
class BLEDescriptor;
class BLEDescriptorCallbacks { public: virtual ~BLEDescriptorCallbacks()=default; virtual void onWrite(BLEDescriptor *){} };
class BLEDescriptor { public: BLEDescriptorCallbacks *callbacks=nullptr; void setCallbacks(BLEDescriptorCallbacks *c) { callbacks=c; } };
class BLE2902 : public BLEDescriptor { bool enabled=false; public: bool getNotifications() { return enabled; } void setNotifications(bool b) { enabled=b; } };
class BLECharacteristic;
class BLECharacteristicCallbacks { public: virtual ~BLECharacteristicCallbacks()=default; virtual void onWrite(BLECharacteristic *){} };
inline std::vector<std::string> fakeNotifications;
inline uint16_t fakePeerMtu=23;
class BLECharacteristic {
 public:
  static constexpr int PROPERTY_READ=1, PROPERTY_WRITE=2, PROPERTY_WRITE_NR=4, PROPERTY_NOTIFY=8;
  BLECharacteristicCallbacks *callbacks=nullptr;
  BLEDescriptor *descriptor=nullptr;
  std::string value;
  void setCallbacks(BLECharacteristicCallbacks *c) { callbacks=c; }
  void addDescriptor(BLEDescriptor *d) { descriptor=d; }
  void setValue(const char *s) { value=s; }
  void setValue(uint8_t *p,size_t n) { value.assign(reinterpret_cast<char *>(p),n); }
  std::string getValue() { return value; }
  void notify() { assert(value.size()<=size_t(fakePeerMtu-3)); fakeNotifications.push_back(value); }
};
class BLEService { public: BLECharacteristic characteristic; BLECharacteristic *createCharacteristic(const char *,int) { return &characteristic; } void start(){} };
class BLEAdvertising { public: int starts=0; void addServiceUUID(const char *){} void setScanResponse(bool){} void start(){ ++starts; } };
class BLEServer { public: BLEServerCallbacks *callbacks=nullptr; BLEService service; void setCallbacks(BLEServerCallbacks *c) { callbacks=c; } BLEService *createService(const char *) { return &service; } void startAdvertising(){} };
class BLEDevice { public: static inline BLEServer server; static inline BLEAdvertising advertising; static void init(const char *){} static void setMTU(int){} static BLEServer *createServer() { return &server; } static BLEAdvertising *getAdvertising() { return &advertising; } };
`````

## 파일: test/stubs/BLEServer.h

SHA-256: `030c5b5382d1d64e47ca0f024bbc274036291739e87cde2c1bce03bb6f6b72a6`

`````cpp
#pragma once
#include "BLEDevice.h"
`````

## 파일: test/stubs/LittleFS.h

SHA-256: `10ba4e0280099a65fd668531f7b48f014941209132c4a6206a4dd45ccfe18bf4`

`````cpp
#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
#include <memory>
#define FILE_READ "r"
#define FILE_WRITE "w"
inline int writeBudget=-1;
inline bool renameFails=false, corruptTempRead=false;
class File {
  std::shared_ptr<std::vector<uint8_t>> bytes;
  size_t pos=0;
  bool temp=false;
 public:
  File() = default;
  explicit File(std::shared_ptr<std::vector<uint8_t>> data,bool isTemp=false):bytes(data),temp(isTemp){}
  explicit operator bool() const { return bool(bytes); }
  size_t size() const { return bytes ? bytes->size() : 0; }
  size_t read(uint8_t *out,size_t n) { n=std::min(n,size()-pos); for(size_t i=0;i<n;++i) out[i]=(*bytes)[pos++]; return n; }
  int read() { if(!available()) return -1; auto b=(*bytes)[pos++]; return corruptTempRead && temp && pos==13 ? b^1 : b; }
  size_t write(const uint8_t *data,size_t n) {
    if(writeBudget>=0) { n=std::min(n,size_t(writeBudget)); writeBudget-=n; }
    bytes->insert(bytes->end(),data,data+n); pos+=n; return n;
  }
  bool available() const { return bytes && pos<bytes->size(); }
  void seek(size_t offset) { pos=offset; }
  void flush() {}
  void close() {}
};
struct FakeFS {
  bool mounted=true;
  int formats=0;
  std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;
  bool begin(bool) { return mounted; }
  bool format() { ++formats; mounted=true; files.clear(); return true; }
  void end() { mounted=false; }
  bool exists(const char *p) { return files.count(p); }
  File open(const char *p,const char *mode) {
    if(std::string(mode)==FILE_WRITE) files[p]=std::make_shared<std::vector<uint8_t>>();
    if(!files.count(p)) return {};
    return File(files[p],std::string(p)=="/profiles.tmp");
  }
  bool rename(const char *from,const char *to) {
    if(renameFails || !files.count(from)) return false;
    files[to]=files[from]; files.erase(from); return true;
  }
};
inline FakeFS LittleFS;
`````

## 파일: test/stubs/MFRC522.h

SHA-256: `bbc184938015db818bc4663d046c91511ca63045a2342cc56bd63cd18b24f13e`

`````cpp
#pragma once
#include "Arduino.h"
class MFRC522 {
 public:
  enum { CommandReg, VersionReg, TxASKReg, TxControlReg, TxModeReg, RxModeReg,
         ModWidthReg, TModeReg, TPrescalerReg, TReloadRegH, TReloadRegL, ModeReg };
  enum StatusCode { STATUS_OK, STATUS_COLLISION, STATUS_TIMEOUT };
  static constexpr uint8_t PCD_SoftReset=0x0F;
  uint8_t registers[16]={}; int resets=0;
  struct { uint8_t size=4, uidByte[10]={1,2,3,4}; } uid;
  MFRC522(uint8_t,uint8_t) { registers[VersionReg]=0x92; }
  void PCD_WriteRegister(uint8_t reg,uint8_t value) {
    registers[reg]=value; if(reg==CommandReg && value==PCD_SoftReset) ++resets;
  }
  uint8_t PCD_ReadRegister(uint8_t reg) { return registers[reg]; }
  void PCD_AntennaOn() { registers[TxControlReg]|=3; }
  StatusCode PICC_WakeupA(byte *,byte *) { return STATUS_TIMEOUT; }
  bool PICC_ReadCardSerial() { return false; }
  void PICC_HaltA() {}
  void PCD_StopCrypto1() {}
};
`````

## 파일: test/stubs/SPI.h

SHA-256: `b0840d50134e2f2d47ca835e99944bf04e222ea78b702c4252d16779b49d8883`

`````cpp
#pragma once
struct FakeSPI {
  int begins=0, ends=0;
  void begin(int,int,int,int) { ++begins; }
  void end() { ++ends; }
};
inline FakeSPI SPI;
`````

## 파일: test/stubs/esp_partition.h

SHA-256: `748e3bcd06bf486b50951991d7769ca29eb18ffcbd2977af48d8ba1845067d04`

`````cpp
#pragma once
#include <cstddef>
#include <cstring>
constexpr int ESP_PARTITION_TYPE_DATA=1, ESP_PARTITION_SUBTYPE_DATA_SPIFFS=2, ESP_OK=0;
struct esp_partition_t { size_t size=256; };
inline bool blankPartition=true;
inline const esp_partition_t *esp_partition_find_first(int,int,const char *) { static esp_partition_t part; return &part; }
inline int esp_partition_read(const esp_partition_t *,size_t,void *buffer,size_t n) { memset(buffer,blankPartition?0xff:0x42,n); return ESP_OK; }
`````

## 파일: test/test_ble.cpp

SHA-256: `1f085b22ab584fe3d00fce0b317d40ea6928060c4335f4f1fdf35af03b268773`

`````cpp
#include <cassert>
#include <string>
#include "../src/ble_link.cpp"
void tick(uint32_t delta=15) { fakeMillis+=delta; bleTick(); }
void write(const std::string &s) { characteristic->value=s; characteristic->callbacks->onWrite(characteristic); }
void connect(uint16_t peer=185) {
  server->callbacks->onConnect(server); fakePeerMtu=peer;
  esp_ble_gatts_cb_param_t param; param.mtu.mtu=peer; server->callbacks->onMtuChanged(server,&param);
  cccd->setNotifications(true); cccd->callbacks->onWrite(cccd);
}
int main() {
  bleBegin(); uint32_t firstSession=bleSessionEpoch();
  connect(); assert(bleSessionEpoch()!=firstSession); assert(bleTakeSync()); assert(!bleTakeSync());
  sendMessage("포고핀 분리상태"); tick();
  assert(fakeNotifications.back()=="포고핀 분리상태\n"); // complete UTF8, one notify
  sendMessage("SENSOR:MPU:FAULT"); tick();
  assert(fakeNotifications.back()=="SENSOR:MPU:FAULT\n");
  assert(std::string(nav::serialEventText("SENSOR:MPU:FAULT").c_str()).find("가속도 MPU6050")!=std::string::npos);
  assert(std::string(nav::serialEventText("REG,OK,AABB,UPDATE,ALICE").c_str()).find("ALICE (기존 카드 갱신)")!=std::string::npos);
  write("CFG,ALICE,80,"); write("17,1\nREG\n");
  String command; assert(blePopCommand(command) && command=="CFG,ALICE,80,17,1");
  assert(blePopCommand(command) && command=="REG"); assert(!blePopCommand(command));
  write(std::string(97,'X')+"\nPING\n"); tick();
  assert(fakeNotifications.back()=="ERR,COMMAND,TOO_LONG\n");
  assert(blePopCommand(command) && command=="PING");
  write("REG\n"); sendMessage("OLD_EVENT");
  uint32_t priorSession=bleSessionEpoch();
  server->callbacks->onDisconnect(server); assert(!bleConnected() && bleSessionEpoch()!=priorSession);
  connect(); tick();
  assert(!blePopCommand(command)); assert(fakeNotifications.back()!="OLD_EVENT\n");
  server->callbacks->onDisconnect(server); tick(); sendMessage("FALL_DETECTED");
  connect(23); tick(); assert(fakeNotifications.back()=="FALL_DETECTED\n");
  sendMessage("포고핀 분리상태"); tick(); assert(fakeNotifications.back()=="ERROR:MTU_LOW\n");
  sendMessage("FALL_DETECTED"); tick(); assert(fakeNotifications.back()=="FALL_DETECTED\n");
  esp_ble_gatts_cb_param_t param; fakePeerMtu=185; param.mtu.mtu=185; server->callbacks->onMtuChanged(server,&param);
  tick(); assert(fakeNotifications.back()=="포고핀 분리상태\n");
  server->callbacks->onDisconnect(server); tick(); connect(23); bleEnableStream();
  write("REG"); assert(!blePopCommand(command));
  write("_CANCEL\n"); assert(blePopCommand(command) && command=="REG_CANCEL");
  size_t before=fakeNotifications.size();
  const std::string message="포고핀 분리상태";
  sendMessage(message.c_str()); tick(); // first UTF8 fragment may end mid-character
  sendMessage("FALL_DETECTED"); tick(); tick();
  std::string stream;
  for(size_t i=before;i<fakeNotifications.size();++i) stream+=fakeNotifications[i];
  assert(stream==message+"\nFALL_DETECTED\n"); // FALL must not corrupt an in-progress frame
  server->callbacks->onDisconnect(server); tick();
  write("CFG,BAD,80,"); connect(); write("REG\n");
  assert(blePopCommand(command) && command=="REG"); // session discards unfinished input
  // Exactly 5 minutes is expired; the preceding millisecond is still deliverable.
  server->callbacks->onDisconnect(server); tick(); sendMessage("FALL_DETECTED");
  uint32_t eventAt=fakeMillis; fakeMillis=eventAt+cfg::FALL_PENDING_MAX_MS-1;
  connect(23); size_t recentStart=fakeNotifications.size(); bleTick();
  assert(fakeNotifications.size()==recentStart+1 && fakeNotifications.back()=="FALL_DETECTED\n");
  server->callbacks->onDisconnect(server); tick(); sendMessage("FALL_DETECTED");
  eventAt=fakeMillis; fakeMillis=eventAt+cfg::FALL_PENDING_MAX_MS;
  connect(23); size_t expiredStart=fakeNotifications.size(); bleTick(); tick();
  for(size_t i=expiredStart;i<fakeNotifications.size();++i) assert(fakeNotifications[i]!="FALL_DETECTED\n");
  assert(!fallPending);
  // Expire offline before a later reconnect or uint32_t clock rollover.
  server->callbacks->onDisconnect(server); tick(); fakeMillis=0xFFFFFF00u;
  sendMessage("FALL_DETECTED"); eventAt=fakeMillis;
  fakeMillis=uint32_t(eventAt+cfg::FALL_PENDING_MAX_MS-1); bleTick(); assert(fallPending);
  fakeMillis=uint32_t(eventAt+cfg::FALL_PENDING_MAX_MS); bleTick(); assert(!fallPending);
  connect(23); expiredStart=fakeNotifications.size(); tick();
  for(size_t i=expiredStart;i<fakeNotifications.size();++i) assert(fakeNotifications[i]!="FALL_DETECTED\n");
  // A newer fall replaces the old timestamp rather than inheriting its deadline.
  server->callbacks->onDisconnect(server); tick(); sendMessage("FALL_DETECTED");
  fakeMillis+=cfg::FALL_PENDING_MAX_MS-1; sendMessage("FALL_DETECTED"); eventAt=fakeMillis;
  fakeMillis=eventAt+1; bleTick(); assert(fallPending);
  connect(23); recentStart=fakeNotifications.size(); tick();
  assert(fakeNotifications.size()==recentStart+1 && fakeNotifications.back()=="FALL_DETECTED\n");
  // Connected but CCCD disabled: expire there too, and never corrupt a long frame.
  cccd->setNotifications(false); sendMessage("FALL_DETECTED");
  fakeMillis+=cfg::FALL_PENDING_MAX_MS; bleTick(); assert(!fallPending);
  cccd->setNotifications(true); bleEnableStream();
  sendMessage("포고핀 분리상태"); tick(); sendMessage("FALL_DETECTED");
  fakeMillis+=cfg::FALL_PENDING_MAX_MS; expiredStart=fakeNotifications.size(); bleTick(); tick();
  for(size_t i=expiredStart;i<fakeNotifications.size();++i) assert(fakeNotifications[i]!="FALL_DETECTED\n");
  puts("BLE: framing, fall priority, 5min expiry/offline/CCCD/wrap/replacement, epochs and input limits PASS");
}
`````

## 파일: test/test_expansion.cpp

SHA-256: `eb5a82c69baeb84e437d8fb6f992eb073a5f5acd3e4b9092b884423d6aff4494`

`````cpp
#include <cassert>
#include <vector>
#include <string>
#include "profiles.h"
#include "feedback.h"
std::vector<std::string> expansionMessages;
int registrationCancels=0;
void sendMessage(const String &s) { expansionMessages.emplace_back(s.c_str()); }
void feedbackStart(Feedback) {}
bool profilesWaiting() { return false; }
void profilesCard(const uint8_t *,uint8_t) {}
void profilesCancelRegistration(const char *) { ++registrationCancels; }
#include "../src/expansion.cpp"
int main() {
  expansionBegin(); fakeAdcMv=1650;
  fakeMillis=10; expansionTick(0); fakeMillis=310; expansionTick(0);
  assert(expansionModule()==nav::Module::Remote);
  fakeMillis=400; recordEcho(nav::EchoResult::Distance,6400,fakeMillis);
  fakeMillis=500; recordEcho(nav::EchoResult::Distance,6000,fakeMillis);
  assert(expansionUpperAlert()); float held=expansionUpperDistance(); assert(held>0);
  fakeMillis=600; recordEcho(nav::EchoResult::Far,38000,fakeMillis);
  assert(expansionUltrasonicCm()==-1 && expansionUpperAlert() && expansionUpperDistance()==held);
  fakeMillis=700; recordEcho(nav::EchoResult::NoResponse,0,fakeMillis);
  assert(expansionUpperAlert() && expansionUpperDistance()==held);
  fakeMillis=800; recordEcho(nav::EchoResult::Far,38000,fakeMillis);
  assert(expansionUpperAlert()); // invalid sample interrupted consecutive clearing
  fakeMillis=900; recordEcho(nav::EchoResult::Far,38000,fakeMillis);
  assert(!expansionUpperAlert() && expansionUpperDistance()==-1 && !expansionFault());
  fakeMillis=1000; recordEcho(nav::EchoResult::Distance,6000,fakeMillis);
  fakeMillis=1100; recordEcho(nav::EchoResult::Distance,6000,fakeMillis);
  for(int i=1;i<=5;++i) { fakeMillis=1100+i*100; recordEcho(nav::EchoResult::StuckHigh,0,fakeMillis); }
  assert(expansionUpperAlert());
  fakeMillis=1700; recordEcho(nav::EchoResult::StuckHigh,0,fakeMillis);
  assert(!expansionUpperAlert()); // invalid samples cannot keep stale haptics alive
  for(int i=0;i<4;++i) recordEcho(nav::EchoResult::StuckHigh,0,fakeMillis);
  assert(expansionFault());
  recordEcho(nav::EchoResult::Far,38000,fakeMillis);
  assert(!expansionFault() && std::string(expansionEchoState())=="FAR");
  // A one-poll ID glitch makes all shared pins input without resetting a live RC522.
  gate=nav::SlotGate{}; observed=nav::Module::None; highZ();
  fakeAdcMv=595; fakeMillis=2000; expansionTick(0); fakeMillis=2300; expansionTick(0);
  fakeMillis=2360; expansionTick(0); assert(expansionRfidReady());
  int resets=rfid.resets, cancels=registrationCancels;
  fakeAdcMv=0; fakeMillis=2365; expansionTick(0);
  assert(!expansionRfidReady() && expansionModule()==nav::Module::Rfid);
  assert(fakePinModes[cfg::S1]==INPUT && fakePinModes[cfg::S2]==INPUT && fakePinModes[cfg::S4]==INPUT);
  fakeAdcMv=595; fakeMillis=2370; expansionTick(0); fakeMillis=2400; expansionTick(0);
  assert(expansionRfidReady() && rfid.resets==resets && registrationCancels==cancels);
  fakeAdcMv=3300; fakeMillis=2410; expansionTick(0); fakeMillis=2510; expansionTick(0);
  assert(expansionModule()==nav::Module::None && registrationCancels==cancels+1);
  puts("expansion: confirmed upper clearing, held warning distance, stale/fault release, far recovery, slot high-Z and reset-free resume PASS");
}
`````

## 파일: test/test_feedback.cpp

SHA-256: `87b3972c8ccdc8479ef6ee37cf88787e2467a26a9a22eeb7f4bd9ab150a1f5c2`

`````cpp
#include <cassert>
#include "profiles.h"
#include "config.h"
float activeObstacleThresholdCM=50, activeStairThresholdCM=170;
uint32_t activeVibDuty=255;
char activeProfileName[13]="DEFAULT";
#include "../src/feedback.cpp"
int main() {
  feedbackBegin(); feedbackTick(false,-1,-1,-1);
  assert(fakeDuty[cfg::VIB_CH_MAIN]==0 && fakeDuty[cfg::VIB_CH_LEFT]==0);
  feedbackStart(Feedback::Fault); feedbackStart(Feedback::Fault);
  feedbackTick(false,-1,-1,-1);
  assert(pattern==nullptr && fakeDuty[cfg::VIB_CH_MAIN]==0);
  assert(fakeDuty[cfg::VIB_CH_LEFT]==0 && fakeDuty[cfg::VIB_CH_RIGHT]==0);
  feedbackStart(Feedback::Green); const uint32_t greenAt=at;
  fakeMillis+=50; feedbackStart(Feedback::Fault); feedbackTick(false,-1,-1,-1);
  assert(pattern==green && at==greenAt && fakeDuty[cfg::VIB_CH_MAIN]==255);
  feedbackTick(true,-1,-1,-1); feedbackTick(false,-1,-1,-1);
  feedbackTick(false,-1,-1,110);
  assert(fakeDuty[cfg::VIB_CH_LEFT]==255 && fakeDuty[cfg::VIB_CH_RIGHT]==255);
  assert(fakeDuty[cfg::VIB_CH_MAIN]==0);
  feedbackStart(Feedback::Green); feedbackTick(true,-1,-1,-1);
  assert(fakeDuty[cfg::VIB_CH_MAIN]==255 && pattern==nullptr);
  brakeSet(true); uint32_t appliedBrake=fakeDuty[cfg::SERVO_CHANNEL];
  feedbackStart(Feedback::Fall); feedbackTick(true,-1,-1,-1);
  assert(pattern==fall && fakeDuty[cfg::VIB_CH_MAIN]==255);
  feedbackStart(Feedback::Fault); assert(pattern==fall);
  fakeMillis+=700; feedbackTick(true,-1,-1,-1);
  assert(fakeDuty[cfg::VIB_CH_MAIN]==0 && fakeDuty[cfg::SERVO_CHANNEL]==appliedBrake);
  fakeMillis+=200; feedbackTick(true,-1,-1,-1); assert(fakeDuty[cfg::VIB_CH_MAIN]==255);
  fakeMillis+=700; feedbackTick(true,-1,-1,-1); assert(fakeDuty[cfg::VIB_CH_MAIN]==0);
  fakeMillis+=200; feedbackTick(true,-1,-1,-1); assert(fakeDuty[cfg::VIB_CH_MAIN]==255);
  fakeMillis+=700; feedbackTick(true,-1,-1,-1); assert(pattern==nullptr && fakeDuty[cfg::VIB_CH_MAIN]==255);
  feedbackTick(false,-1,-1,-1); assert(fakeDuty[cfg::VIB_CH_MAIN]==0);
  brakeSet(false); assert(fakeDuty[cfg::SERVO_CHANNEL]!=appliedBrake);
  puts("feedback: silent faults, uninterrupted green/fall, upper both-sides, stair caution, retained brake and pattern completion PASS");
}
`````

## 파일: test/test_logic.cpp

SHA-256: `96577a8968ce09320a850ee362b7991d56ebd9e5cfb151062a274c02ad3841aa`

`````cpp
#include "logic.h"
#include <assert.h>
#include <stdio.h>
using nav::Module;
int main() {
  const uint8_t pins[] = {cfg::OBSTACLE_SDA,cfg::OBSTACLE_SCL,cfg::OBSTACLE_LPN,
    cfg::SEGMENT_SDA,cfg::SEGMENT_SCL,cfg::VIB_MAIN,cfg::VIB_LEFT,cfg::VIB_RIGHT,
    cfg::SERVO,cfg::BUTTON,cfg::UART_RX,cfg::UART_TX,cfg::SLOT_ID,cfg::S1,cfg::S2,cfg::S3,cfg::S4};
  for(size_t i=0;i<sizeof(pins);++i) for(size_t j=i+1;j<sizeof(pins);++j) assert(pins[i]!=pins[j]);
  assert(nav::identify(595)==Module::Rfid);
  assert(nav::identify(1650)==Module::Remote);
  assert(nav::identify(2533)==Module::Led);
  assert(nav::identify(3300)==Module::None);
  assert(nav::identify(0)==Module::Unknown);
  assert(nav::identify(1000)==Module::Unknown);
  nav::ObstacleRecovery recovery;
  assert(recovery.shouldInitialize(0));
  recovery.initialized(false,1000);
  assert(!recovery.shouldInitialize(5999) && recovery.shouldInitialize(6000));
  recovery.initialized(true,7000); recovery.frame(7100);
  assert(nav::elapsed(7700,7100,cfg::OBSTACLE_STALE_MS));
  assert(!recovery.needsRestart(7700) && recovery.running()); // do not reset for a 600ms gap
  recovery.frame(8000); assert(!recovery.needsRestart(10499));
  assert(recovery.needsRestart(10500) && recovery.shouldInitialize(10500));
  recovery.initialized(false,14000);
  assert(!recovery.shouldInitialize(18999) && recovery.shouldInitialize(19000));
  recovery.initialized(true,0xFFFFFF00u); recovery.frame(0xFFFFFFF0u);
  assert(!recovery.needsRestart(uint32_t(0xFFFFFFF0u+2499u)));
  assert(recovery.needsRestart(uint32_t(0xFFFFFFF0u+2500u)));
  assert(recovery.shouldInitialize(uint32_t(0xFFFFFFF0u+2500u)));
  recovery.initialized(false,0xFFFFFF00u);
  assert(!recovery.shouldInitialize(uint32_t(0xFFFFFF00u+4999u)));
  assert(recovery.shouldInitialize(uint32_t(0xFFFFFF00u+5000u)));
  nav::SlotGate gate;
  assert(!gate.update(Module::Rfid,10));
  assert(!gate.update(Module::Rfid,309));
  assert(gate.update(Module::Rfid,310) && gate.permitted);
  // A one-sample glitch inhibits outputs without detach or repeated RFID reset.
  assert(!gate.update(Module::Unknown,315));
  assert(gate.active==Module::Rfid && !gate.permitted);
  gate.update(Module::Rfid,320); assert(!gate.permitted);
  gate.update(Module::Rfid,349); assert(!gate.permitted);
  assert(!gate.update(Module::Rfid,350) && gate.permitted);
  assert(!gate.update(Module::Remote,400) && !gate.permitted);
  assert(!gate.update(Module::Remote,499));
  assert(gate.update(Module::Remote,500) && gate.active==Module::None);
  assert(!gate.update(Module::Remote,699));
  assert(gate.update(Module::Remote,700) && gate.active==Module::Remote);
  // Noise across several ID bands still detaches a persistently unsafe signal.
  gate.update(Module::None,705); gate.update(Module::Unknown,750);
  assert(gate.update(Module::Led,805) && gate.active==Module::None);
  nav::SlotGate wrap;
  wrap.update(Module::Led,0xFFFFFF00u);
  assert(!wrap.update(Module::Led,0x0000002Bu));
  assert(wrap.update(Module::Led,0x0000002Cu));
  wrap.update(Module::None,0xFFFFFFF0u);
  assert(!wrap.update(Module::None,0x00000053u));
  assert(wrap.update(Module::None,0x00000054u));
  nav::ReconnectGate reconnect;
  assert(!reconnect.update(true,0));
  assert(!reconnect.update(true,2999));
  for(int t=3000;t<3800;t+=200) assert(!reconnect.update(true,t));
  assert(reconnect.update(true,3800));
  assert(!reconnect.update(false,4000));
  assert(!reconnect.update(true,4100));
  assert(!reconnect.update(false,7000));
  assert(!reconnect.update(true,7100));
  for(int t=10100;t<10900;t+=200) assert(!reconnect.update(true,t));
  assert(reconnect.update(true,10900));
  using nav::EchoResult;
  assert(nav::classifyEcho(true,true,false,38000)==EchoResult::Far);
  assert(nav::classifyEcho(true,true,false,6000)==EchoResult::Distance);
  assert(nav::classifyEcho(false,false,false,0)==EchoResult::NoResponse);
  assert(nav::classifyEcho(false,true,true,0)==EchoResult::StuckHigh);
  assert(nav::classifyEcho(false,false,true,0)==EchoResult::StuckHigh);
  assert(nav::classifyEcho(true,true,false,10)==EchoResult::Invalid);
  assert(nav::classifyEcho(true,true,false,65000)==EchoResult::Invalid);
  nav::UpperDecision upper;
  upper.sample(119); assert(!upper.alert); upper.sample(110); assert(upper.alert);
  upper.sample(125); upper.sample(125); assert(upper.alert);
  upper.sample(131); assert(upper.alert); upper.sample(-1); assert(!upper.alert);
  upper.reset(); upper.sample(119); upper.sample(125); upper.sample(119);
  assert(!upper.alert); // middle band breaks consecutive detection
  upper.sample(110); assert(upper.alert && upper.warningCm==110);
  upper.sample(-1); assert(upper.alert && upper.warningCm==110);
  upper.invalid(); upper.sample(-1); assert(upper.alert && upper.warningCm==110);
  upper.sample(125); upper.sample(131); assert(upper.alert && upper.warningCm==125);
  upper.sample(125); upper.sample(131); assert(upper.alert); // clear must be consecutive too
  upper.sample(132); assert(!upper.alert && upper.warningCm==-1);
  upper.sample(NAN); assert(!upper.alert);
  assert(nav::normalLeds(true,true,true,true,true)==31);
  assert(nav::normalLeds(false,true,false,false,true)==18);
  assert(nav::normalLeds(false,false,false,false,false)==0);
  nav::Mean3 mean;
  assert(mean.sample(30)==30); assert(mean.sample(60)==45);
  assert(mean.sample(90)==60); assert(mean.sample(-1)==-1); assert(mean.sample(12)==12);
  nav::StairDecision stair;
  stair.sample(175,false,170); assert(!stair.alert);
  stair.sample(180,false,170); assert(stair.alert);
  for(int i=0;i<4;i++) stair.sample(168,false,170);
  assert(stair.alert); // hysteresis retains warning at boundary
  stair.sample(150,false,170); stair.sample(150,false,170); assert(stair.alert);
  stair.sample(150,false,170); assert(!stair.alert);
  stair.sample(-1,true,170); assert(!stair.alert);
  stair.sample(-1,true,170); assert(stair.alert);
  stair.reset(); assert(!stair.alert);
  stair.sample(-1,false,170); stair.sample(-1,false,170); assert(!stair.alert);
  stair.sample(-1,false,170); assert(stair.alert);
  const uint8_t pos[6] = {0,0,0,0,0x10,0};
  const uint8_t neg[6] = {0,0,0,0,0xF0,0};
  assert(fabsf(nav::accelMagnitude(pos)-9.80665f)<0.001f);
  assert(fabsf(nav::accelMagnitude(neg)-9.80665f)<0.001f);
  nav::FallDetector fall;
  assert(!fall.sample(2,100)); assert(!fall.sample(20,200));
  assert(!fall.sample(9.8,700)); assert(!fall.sample(9.8,1699)); assert(fall.sample(9.8,1700));
  fall.sample(2,2000); fall.sample(9.8,2601); assert(!fall.sample(20,2650));
  fall.sample(2,3000); fall.sample(20,3100); fall.reset(); assert(!fall.sample(9.8,5000));
  assert(!fall.sample(NAN,6000));
  nav::FallDetector late;
  late.sample(2,0); late.sample(20,601);
  late.sample(9.8,1101); assert(!late.sample(9.8,2101));
  late.reset(); late.sample(2,3000); late.sample(20,3600);
  late.sample(9.8,4100); assert(!late.sample(9.8,5100)); // exact deadline is expired
  late.reset(); late.sample(2,6000); late.sample(20,6599);
  late.sample(9.8,7099); assert(late.sample(9.8,8099)); // final allowed millisecond
  puts("logic: obstacle recovery/backoff/wrap, ID, detach, debounce, stair and fall PASS");
}
`````

## 파일: test/test_main.cpp

SHA-256: `1780546e1e42e35c2a65181b601d00a368e332277f559c0360c909f26ccc7ebf`

`````cpp
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include "sensors.h"
#include "profiles.h"
#include "expansion.h"
#include "feedback.h"
SensorSnapshot fakeSnapshot;
bool fakeExpansionFault=false;
int faultStarts=0;
std::vector<std::string> mainMessages;
float activeObstacleThresholdCM=50, activeStairThresholdCM=170;
uint32_t activeVibDuty=255;
char activeProfileName[13]="DEFAULT";
SensorSnapshot sensorSnapshot() { return fakeSnapshot; }
bool expansionFault() { return fakeExpansionFault; }
uint16_t expansionIdMv() { return 3300; }
nav::Module expansionModule() { return nav::Module::None; }
bool expansionUpperAlert() { return false; }
const char *expansionEchoState() { return "ABSENT"; }
bool profilesStoreReady() { return true; }
bool bleConnected() { return true; }
void sendMessage(const String &s) { mainMessages.emplace_back(s.c_str()); }
void feedbackStart(Feedback f) { if(f==Feedback::Fault) ++faultStarts; }
#include "../src/main.cpp"
void fresh() {
  fakeSnapshot.mpuReady=fakeSnapshot.stairReady=fakeSnapshot.obstacleReady=true;
  fakeSnapshot.accelSeq=fakeSnapshot.obstacleSeq=1;
  if(!fakeSnapshot.stairSeq) fakeSnapshot.stairSeq=1;
  fakeSnapshot.accelAt=fakeSnapshot.stairAt=fakeSnapshot.obstacleAt=fakeMillis;
  fakeSnapshot.stairCm=150; fakeSnapshot.leftCm=10; fakeSnapshot.rightCm=20;
}
size_t occurrences(const char *line) {
  return std::count(mainMessages.begin(),mainMessages.end(),line);
}
int main() {
  fakeMillis=1000; fresh(); previousHealth=0; consumeSensors();
  fakeMillis+=501; fresh(); fakeExpansionFault=true; consumeSensors();
  assert(faultStarts==0); // expansion.cpp already owns its warning, do not restart it
  fakeMillis+=501; fresh(); fakeExpansionFault=false; consumeSensors();
  fakeMillis+=501; fresh(); fakeSnapshot.obstacleReady=false; consumeSensors();
  assert(faultStarts==1); // an actual new base sensor fault still warns
  fakeMillis+=501; fresh(); fakeSnapshot.obstacleReady=false; consumeSensors();
  assert(faultStarts==1);
  fakeMillis+=501; fresh(); fakeSnapshot.obstacleAt=fakeMillis-599;
  consumeSensors(); assert(obstacleUsable && leftCm==10 && rightCm==20);
  ++fakeMillis; fakeSnapshot.accelAt=fakeSnapshot.stairAt=fakeMillis;
  consumeSensors(); assert(!obstacleUsable && leftCm==-1 && rightCm==-1);
  // The driver remains ready until its separate 2500ms watchdog. Main ignores stale data.
  assert(fakeSnapshot.obstacleReady);
  fakeMillis+=20; fresh(); fakeSnapshot.stairCm=180; ++fakeSnapshot.stairSeq;
  consumeSensors(); assert(!stairAlert);
  fakeMillis+=20; fresh(); fakeSnapshot.stairCm=180; ++fakeSnapshot.stairSeq;
  consumeSensors(); assert(stairAlert && occurrences("계단 감지")==1);
  fakeMillis+=20; fresh(); fakeSnapshot.stairCm=180; ++fakeSnapshot.stairSeq;
  consumeSensors(); assert(occurrences("계단 감지")==1 && occurrences("STAIR_DETECTED")==0);
  status(); assert(occurrences("계단 감지")==2); // explicit state synchronization
  for(int i=0;i<3;++i) { fakeMillis+=20; fresh(); ++fakeSnapshot.stairSeq; consumeSensors(); }
  assert(!stairAlert && occurrences("STAIR_CLEARED")==1);
  fakeSnapshot.mpuReady=true; fakeSnapshot.accelSeq=1;
  fakeSnapshot.accelAt=fakeMillis-120; sensors=fakeSnapshot; mpuUsable=false;
  assert(std::string(mpuStatusText()).find("측정값 갱신 지연")!=std::string::npos);
  fakeSnapshot.mpuReady=false; fakeSnapshot.mpuReason="I2C 응답 부족"; sensors=fakeSnapshot;
  assert(std::string(mpuStatusText())=="I2C 응답 부족");
  fakeSnapshot.stairReady=false; fakeSnapshot.stairReason="미연결 또는 모델 ID 읽기 실패"; stairUsable=false;
  sensors=fakeSnapshot;
  assert(std::string(stairStatusText()).find("미연결")!=std::string::npos);
  puts("main: no duplicate expansion fault, base sensor warning, 599/600ms freshness, Korean stair events and status sync PASS");
}
`````

## 파일: test/test_profiles.cpp

SHA-256: `a3e7422c993bc040aa394c676ecd6f6bf5fa41ddee45916bb78a9e4f16927600`

`````cpp
#include <cassert>
#include <vector>
#include <string>
#include "feedback.h"
#include "expansion.h"
std::vector<std::string> messages;
bool rfidReady=false;
bool fakeLinked=true;
uint32_t fakeSession=1;
bool bleConnected() { return fakeLinked; }
uint32_t bleSessionEpoch() { return fakeSession; }
void sendMessage(const String &s) { messages.emplace_back(s.c_str()); }
void sendProtocolMessage(const String &s) { sendMessage(s); }
void feedbackStart(Feedback) {}
bool expansionRfidReady() { return rfidReady; }
#include "../src/profiles.cpp"
static void expect(const char *line) { assert(!messages.empty() && messages.back()==line); }
int main() {
  LittleFS.mounted=false; profilesBegin(); assert(profileStoreReady && LittleFS.formats==1);
  profilesCommand("CFG,ALICE,80,17,1"); expect("ACK,CFG");
  assert(activeObstacleThresholdCM==50 && pendingCfg.valid);
  profilesCommand("CFG,BAD,201,17,1"); expect("ERR,CFG,RANGE"); assert(std::string(pendingCfg.name)=="ALICE");
  profilesCommand("CFG,A B,80,17,1"); expect("ERR,CFG,FORMAT");
  profilesCommand("REG"); expect("REG,NO_RFID");
  rfidReady=true; profilesCommand("REG"); expect("REG,WAIT,15");
  uint8_t uid[4]={1,2,3,4}; profilesCard(uid,4); expect("REG,OK,01020304,NEW,ALICE");
  assert(profileCount==1 && activeObstacleThresholdCM==80);
  auto durable=*LittleFS.files[PROFILE_FILE_PATH];
  profilesCommand("CFG,BOB,90,18,0"); assert(activeObstacleThresholdCM==80);
  writeBudget=5; profilesCommand("REG"); profilesCard(uid,4); expect("REG,FSERR");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==durable && std::string(profiles[0].name)=="ALICE");
  writeBudget=-1; renameFails=true; profilesCommand("REG"); profilesCard(uid,4); expect("REG,FSERR");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==durable && activeObstacleThresholdCM==80);
  renameFails=false; corruptTempRead=true; profilesCommand("REG"); profilesCard(uid,4); expect("REG,FSERR");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==durable);
  corruptTempRead=false; profilesCommand("REG"); profilesCard(uid,4); expect("REG,OK,01020304,UPDATE,BOB");
  assert(profileCount==1 && activeObstacleThresholdCM==90 && activeVibDuty==cfg::VIB_WEAK_DUTY);
  applyDefaultProfile(); loadProfiles(); assert(profileCount==1 && profileStoreReady);
  fakeMillis+=3000; profilesCard(uid,4); expect("APPLY,01020304,BOB");
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG"); profilesClearPending();
  assert(!profilesWaiting() && !pendingCfg.valid); expect("REG,CANCEL,DISCONNECTED");
  profilesCommand("REG"); expect("REG,NOCFG");
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG"); fakeMillis+=15000; profilesTick(); expect("REG,TIMEOUT");
  auto beforeDeadline=*LittleFS.files[PROFILE_FILE_PATH];
  profilesCommand("REG"); fakeMillis+=15000;
  profilesCard(uid,4); expect("REG,TIMEOUT");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==beforeDeadline);
  assert(std::string(profiles[0].name)=="BOB" && activeObstacleThresholdCM==90);
  auto beforeSession=*LittleFS.files[PROFILE_FILE_PATH];
  profilesCommand("REG"); ++fakeSession; // rapid reconnect, no loop cleanup yet
  profilesCard(uid,4); expect("REG,CANCEL,DISCONNECTED");
  assert(!pendingCfg.valid && *LittleFS.files[PROFILE_FILE_PATH]==beforeSession);
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG"); fakeLinked=false;
  profilesCard(uid,4); expect("REG,CANCEL,DISCONNECTED");
  assert(!pendingCfg.valid && *LittleFS.files[PROFILE_FILE_PATH]==beforeSession);
  fakeLinked=true; ++fakeSession;
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG");
  fakeMillis=0xFFFFFF00u; regStartTime=fakeMillis;
  fakeMillis=uint32_t(regStartTime+14999u); assert(profilesWaiting());
  fakeMillis=uint32_t(regStartTime+15000u); profilesCard(uid,4); expect("REG,TIMEOUT");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==beforeSession);
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG");
  profilesCommand("CFG,DAVE,110,17,1"); expect("ACK,CFG"); assert(!profilesWaiting());
  // CRC failure must keep the damaged file and refuse replacement/false UNREG.
  (*LittleFS.files[PROFILE_FILE_PATH])[12]^=1; auto corrupt=*LittleFS.files[PROFILE_FILE_PATH];
  loadProfiles(); assert(!profileStoreReady && *LittleFS.files[PROFILE_FILE_PATH]==corrupt);
  profilesCommand("REG"); expect("REG,FSERR"); profilesCard(uid,4); expect("REG,FSERR");
  int formats=LittleFS.formats;
  LittleFS.mounted=false; blankPartition=false; loadProfiles(); assert(!profileStoreReady && LittleFS.formats==formats);
  profilesSerialCommand("FS_INIT"); assert(LittleFS.formats==formats);
  profilesSerialCommand("FS_INIT_CONFIRM_ERASE_PROFILES"); assert(profileStoreReady && profileCount==0 && LittleFS.formats==formats+1);
  profilesSerialCommand("FS_INIT_CONFIRM_ERASE_PROFILES"); assert(LittleFS.formats==formats+1);
  puts("profiles: app CFG/REG/APPLY, errors, timeout, session cleanup, short write, CRC, rename failure, mount protection PASS");
}
`````

## 파일: lib/VL53L4CX/LICENSE.md

SHA-256: `d932143f3b8161f5c35c1826f8837c6f3f98a0a32e56c3c340e85daa53428ba0`

`````markdown
COPYRIGHT(c) 2022 STMicroelectronics

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
Neither the name of STMicroelectronics nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
`````

## 파일: lib/SparkFun_VL53L5CX/LICENSE.md

SHA-256: `ef6aa872e1cec61f008808bc061d047bfdd83cbcf3da59c232c8a65f70303f63`

`````markdown
SparkFun License Information
============================

SparkFun uses two different licenses for our files — one for hardware and one for code.

Hardware
---------

**SparkFun hardware is released under [Creative Commons Share-alike 4.0 International](http://creativecommons.org/licenses/by-sa/4.0/).**

Note: This is a human-readable summary of (and not a substitute for) the [license](http://creativecommons.org/licenses/by-sa/4.0/legalcode).

You are free to:

Share — copy and redistribute the material in any medium or format
Adapt — remix, transform, and build upon the material
for any purpose, even commercially.
The licensor cannot revoke these freedoms as long as you follow the license terms.
Under the following terms:

Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.
No additional restrictions — You may not apply legal terms or technological measures that legally restrict others from doing anything the license permits.
Notices:

You do not have to comply with the license for elements of the material in the public domain or where your use is permitted by an applicable exception or limitation.
No warranties are given. The license may not give you all of the permissions necessary for your intended use. For example, other rights such as publicity, privacy, or moral rights may limit how you use the material.


Code
--------

**SparkFun code, firmware, and software is released under the MIT License(http://opensource.org/licenses/MIT).**

The MIT License (MIT)

Copyright (c) 2020 SparkFun Electronics

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
`````
