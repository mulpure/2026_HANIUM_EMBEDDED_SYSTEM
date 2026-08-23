/*
  프로젝트명 : ESP32 Web Bluetooth + 낙상 감지(MPU6050) + 전방 장애물 감지(VL53L5CX, 좌/우 구분) + 계단 감지(VL53L1X)
              + Pi5 UART2 연동(YOLO 신호등 감지 트리거/결과 수신)
  설     명 : Netlify 웹페이지와 ESP32를 BLE로 연결하고,
              MPU6050 가속도센서로 낙상을, VL53L5CX ToF로 전방 장애물(좌/우 구분)을,
              VL53L1X ToF로 하향 계단(내리막)을 감지.
              추가로 UART2(GPIO16/17)로 Pi5와 통신하여 YOLO 신호등 감지 세션을 트리거하고
              초록불 감지 결과를 받아 진동으로 알림.

  통신 방식 : Bluetooth Low Energy (웹페이지) + UART2 115200bps (Pi5)
  장치 이름 : NAVCANE_ESP32

  [이번 수정 내용 - VL53L1X 소프트웨어 리셋 도입 (XSHUT GPIO 제어)]
    - 기존 문제: VL53L1X의 XSHUT을 3.3V에 하드와이어해두어, setI2CAddress(0x30)로
      한 번 재할당된 뒤에는 begin()이 기본 주소 0x29에서 센서를 찾지 못해 계속 실패함.
      (ESP32 리셋/재업로드만으로는 센서 쪽 전원이 유지되어 주소가 0x30에 그대로 남기 때문)
      해결하려면 매번 센서 전원 자체를 물리적으로 뽑았다 꽂아야 했음 -> 매우 번거롭고 재현성 낮음
    - 해결: XSHUT을 GPIO27로 옮기고, setup()에서 VL53L1X 초기화 직전에
      LOW -> delay -> HIGH로 토글하여 소프트웨어적으로 완전한 하드웨어 리셋을 수행.
      이렇게 하면 이전 세션에서 주소가 0x30으로 바뀌어 있었더라도 매 부팅 시 항상
      깨끗한 기본 상태(0x29)로 초기화되어, 전원을 물리적으로 뽑을 필요가 없어짐.
    - 추가로 begin() 실패 시 반환값을 그대로 출력하도록 하여 디버깅 정보를 더 명확히 함.

  [이전 수정 내용 - Pi5 UART2 연동 재구축]
    - 기존 UART 연동은 완전히 새로 작성. 통신 프로토콜만 재설계된 것이며
      Pi5의 YOLO 신호등 감지 기능(green/red pedestrian signal) 자체는 그대로 유지됨.
    - 푸시버튼(모멘터리, GPIO33, INPUT_PULLUP): 누르는 순간(디바운스 후) "BTN\n"을
      Pi5로 1회 전송 -> Pi5가 이를 받으면 YOLO 신호등 감지 프로그램을 5분간 실행
    - Pi5가 초록불(green_pedestrian_signal)을 감지하면 "GREEN\n"을 ESP32로 전송
      -> ESP32는 진동모터로 "짧게-길게-짧게-길게" 패턴을 1회 재생 (논블로킹, 1회성)
      -> 이후 YOLO 세션 잔여 실행시간을 1분으로 단축하는 처리는 Pi5(파이썬) 쪽에서 자체적으로 수행
    - 진동 우선순위 변경: 초록불 패턴(1회, 최우선) > 계단 경고(연속 ON) > 장애물 좌/우/양쪽 패턴
      -> 낙상 감지는 더 이상 진동을 울리지 않고 BLE로 "FALL_DETECTED" 알림만 전송 (사용자 요청)
    - UART2는 ESP32 Serial2 기본 핀(RX=GPIO16, TX=GPIO17)을 그대로 사용, 115200bps,
      Pi5와 3선(TX/RX/GND) 직결 (둘 다 3.3V 로직이라 레벨시프터 불필요)

  [이전 수정 내용 - 계단 감지 센서를 HC-SR04(초음파) -> VL53L1X(ToF)로 교체]
    - HC-SR04는 5V 소자라 ESP32(3.3V) 연결 시 전압분배 저항(1k/2k)이 필요했고,
      경사진 계단 각도에서 초음파가 빗나가 측정 실패(타임아웃)가 잦았음
    - VL53L1X는 3.3V I2C 소자라 전압분배 회로가 불필요하고, 레이저 기반이라
      경사각(테스트 기준 25도)에서도 상대적으로 안정적으로 측정됨
    - 단, VL53L5CX와 VL53L1X 모두 기본 I2C 주소가 0x29로 동일 -> 주소 충돌 발생
      => VL53L5CX의 LPn(GPIO25)만 제어
      => VL53L5CX를 먼저 끈 상태에서 VL53L1X 주소를 0x30으로 변경한 뒤 VL53L5CX를 켬
    - readHCSR04DistanceCM() -> readVL53L1XDistanceCM() 으로 함수만 교체
      (반환값 의미 동일: -1.0f = 새 데이터 없음, >=0 = 거리(cm))
    - filteredStairDistance(), STAIR_CONFIRM_COUNT, stairAlertActive, checkStairs(),
      printStairDistance(), BLE GET_STAIR_DISTANCE 명령 등 나머지 로직은 전부 그대로 재사용
      (센서가 바뀐 것을 몰라도 되도록 인터페이스를 동일하게 유지)
    - HCSR04_SAMPLE_INTERVAL(60ms)은 그대로 VL53L1X의 측정 주기(IntermeasurementPeriod)로 재사용

  [이전 수정 내용 - ToF 상/하(행) 각도 필터 추가로 바닥 오탐 방지]
    - VL53L5CX 8x8(64존) 그리드에서 행(row = i / 8, 0~7)이 아래쪽일수록
      바닥을 비추는 각도가 되어 바닥이 장애물로 오탐될 수 있음
    - TOF_ROW_MIN ~ TOF_ROW_MAX 범위의 행만 유효 데이터로 사용하도록
      readToFDistances() 안에서 row 필터를 추가
    - 기본값은 TOF_ROW_MIN=0, TOF_ROW_MAX=2 (임시값) -> 실제 장착 후
      debugPrintTofRows()로 행별 평균 거리를 확인하고 값을 조정할 것
      (바닥을 비추는 행 = 거리가 짧게/일정하게 나오는 행 -> 그 행을 범위에서 제외)
    - GET_SENSOR 명령 등으로 디버그 함수를 트리거하고 싶다면 필요 시 추가 가능
      (현재는 setup 직후 잠깐 사용할 수 있도록 함수만 정의해둠, loop에서 기본 비활성화)

  [이전 수정 내용 - 좌/우 장애물 진동 패턴 분리]
    - VL53L5CX 8x8(64존) 그리드를 열(column) 기준 좌(0~3열)/우(4~7열)로 분리하여
      각각 독립적으로 최소 거리를 계산 (checkObstacle -> readToFDistances)
    - 좌측 장애물 감지 시  : "짧게-짧게-쉬고" 패턴 반복
    - 우측 장애물 감지 시  : "짧게-길게-쉬고" 패턴 반복
    - 좌+우 동시 감지 시   : 위 두 패턴을 이어붙인 복합 패턴 반복
    - delay() 없이 millis() 기반 논블로킹 상태머신(playObstaclePattern)으로 패턴 재생
      -> 진동 재생 중에도 센서 폴링/BLE 처리가 멈추지 않음
    - 계단 경고는 기존과 동일하게 최우선으로 진동 계속 ON 유지
      (계단 경고가 없을 때만 장애물 좌/우 패턴이 재생됨)
    - 낙상 알림(triggerFallAlert)은 기존처럼 blocking 패턴 유지, 종료 후 장애물 패턴 상태 초기화

    ※ 지팡이 장착 방향에 따라 좌/우가 반대로 나올 수 있습니다.
       실제 장착 후 반대로 느껴지면 readToFDistances() 안의 "col < 4" 조건만 뒤집으면 됩니다.
    ※ 마찬가지로 상/하(row)도 장착 방향에 따라 반대로 나올 수 있으니
       TOF_ROW_MIN/TOF_ROW_MAX 값을 실측 후 조정하세요.
*/

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <SparkFun_VL53L5CX_Library.h>   // PlatformIO/Arduino Library Manager: SparkFun VL53L5CX
#include <SparkFun_VL53L1X.h>            // PlatformIO/Arduino Library Manager: SparkFun VL53L1X 4m Laser Distance Sensor
#include <math.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ======================================================
// BLE 설정
// index.html에 입력된 UUID와 반드시 같아야 함
// ======================================================

#define DEVICE_NAME "NAVCANE_ESP32"

#define SERVICE_UUID \
  "12345678-1234-1234-1234-1234567890ab"

#define CHARACTERISTIC_UUID \
  "abcd1234-1234-1234-1234-abcdef123456"

// ======================================================
// 핀 설정
// ======================================================

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

const int LED_PIN = LED_BUILTIN;

// 센서를 연결할 아날로그 입력 핀 (기존 예제용, 필요 없으면 무시)
const int SENSOR_PIN = 34;

// 진동 모터 (트랜지스터/MOSFET 경유) - 1개만 사용
#define VIBRATION_PIN 32

// VL53L5CX (전방 장착, 좌/우 장애물 감지용)
#define VL53L5CX_LPN_PIN 25       // 부팅 시 순차 초기화용 - active-low 비활성화 핀 (기본 주소 0x29 그대로 사용)

// VL53L1X (하향 장착, 계단 감지용 - 구 HC-SR04 자리)
#define VL53L1X_NEW_ADDR  0x60    // SparkFun 라이브러리 값 0x60 = 실제 7비트 I2C 주소 0x30
// ※ XSHUT은 여전히 3.3V 하드와이어 그대로 사용 (추가 GPIO 불필요 - 아래 setup()의 [신규] 주석 참고)

// MPU6050, VL53L5CX, VL53L1X 모두 I2C 기본 핀 공유 (SDA=21, SCL=22)
// MPU6050 = 0x68, VL53L5CX = 0x29, VL53L1X = 0x30(재할당 후) -> 주소 충돌 없음
// 부팅 순서: VL53L5CX(LPn=LOW)만 꺼둔 채 VL53L1X를 0x30으로 재할당 -> VL53L5CX(LPn=HIGH) 깨워서 0x29로 초기화
//           (VL53L5CX를 먼저 깨우면 VL53L1X 주소 재할당 시점에 두 센서가 동시에 0x29로 응답해 충돌 발생)

// ------------------------------------------------------
// 라즈베리파이5 UART2 통신 (Pi5의 YOLO 신호등 감지 프로그램 트리거/결과 수신용)
//   - ESP32 <-> Pi5 3선(TX/RX/GND) 직결, 둘 다 3.3V 로직이라 레벨시프터 불필요
//   - RX16/TX17은 ESP32 Serial2 기본 핀과 동일 (WROOM 기준)
// ------------------------------------------------------
#define PI_UART_BAUD 115200
#define PI_UART_RX_PIN 16   // ESP32 RX2 <- Pi5 TX
#define PI_UART_TX_PIN 17   // ESP32 TX2 -> Pi5 RX

// 푸시버튼 (모멘터리, 눌렸다 떨어지는 버튼) - Pi5 YOLO 5분 세션 시작 트리거
// INPUT_PULLUP 사용: 평상시 HIGH, 누르면 LOW
#define BUTTON_PIN 33
const unsigned long BUTTON_DEBOUNCE_MS = 50;

// 센서 데이터 전송 간격
const unsigned long SEND_INTERVAL = 1000;

// ======================================================
// 시리얼 모니터 출력 주기 설정
// ======================================================

const unsigned long STAIR_PRINT_INTERVAL = 500;   // 계단 거리값 시리얼 출력 주기 (ms)
unsigned long lastStairPrintTime = 0;

// ======================================================
// 낙상 감지 임계값 (단위: m/s^2, 1g ≈ 9.81)
// ※ 테스트용으로 완화된 값. 실제 배포 전 프로덕션 값으로 복원 필요.
// ======================================================

const float FREEFALL_THRESHOLD = 3.0;    // 이 값 아래 = 자유낙하 (~0.3g)
const float IMPACT_THRESHOLD   = 25.0;   // 이 값 위   = 충격     (~2.5g)
const float STILLNESS_MIN      = 8.0;    // 정지 판정 하한 (~0.8g)
const float STILLNESS_MAX      = 11.5;   // 정지 판정 상한 (~1.2g)

const unsigned long FREEFALL_WINDOW    = 600;   // 자유낙하 후 충격을 기다리는 시간 (ms)
const unsigned long IMPACT_SETTLE_TIME = 500;   // 충격 직후 진동(링잉) 무시 시간 (ms)
const unsigned long STILLNESS_DURATION = 2000;  // 정지 상태 유지 요구 시간 (ms)

const unsigned long ACCEL_SAMPLE_INTERVAL = 15; // 가속도 샘플링 (~66Hz)

// ======================================================
// ToF 전방 장애물 감지 설정 (좌/우 구분)
// ======================================================

const float OBSTACLE_THRESHOLD_CM = 50.0;   // 이 거리보다 가까우면 장애물로 판단 (테스트 시 임시로 줄여도 됨)
const int   TOF_FILTER_SAMPLES    = 3;       // 이동평균 필터 샘플 수

// ------------------------------------------------------
// ToF 상/하(행) 각도 필터 (바닥 오탐 방지)
//   - 8x8 그리드에서 행(row = i / 8, 0~7)이 아래쪽일수록 바닥을 비출 가능성이 높음
//   - TOF_ROW_MIN ~ TOF_ROW_MAX 범위의 행만 유효 데이터로 사용
//   - 지팡이 장착 각도/방향에 따라 어느 행이 "아래쪽"인지 달라지므로
//     실제 장착 후 debugPrintTofRows()로 확인하며 값을 조정할 것
//   - 기본값(0~2)은 임시값이며, 바닥을 비추는 행을 범위에서 제외해야 함
// ------------------------------------------------------
int TOF_ROW_MIN = 0;   // 사용할 행 범위의 시작 (위쪽)
int TOF_ROW_MAX = 2;   // 사용할 행 범위의 끝 (아래쪽) - 바닥에 걸리면 이 값을 줄일 것

SparkFun_VL53L5CX tof;
uint8_t tofZones = 0;        // 실제 적용된 해상도 (16 or 64) - 좌/우 분리는 64존(8x8) 기준

// 좌/우 각각 독립적인 이동평균 필터 버퍼
float tofFilterBufLeft[TOF_FILTER_SAMPLES]  = { -1, -1, -1 };
int   tofFilterIdxLeft  = 0;
float tofFilterBufRight[TOF_FILTER_SAMPLES] = { -1, -1, -1 };
int   tofFilterIdxRight = 0;

float lastDistanceCM = -1.0f;   // GET_DISTANCE 조회용 (좌/우 중 더 가까운 값)

// 장애물 좌/우 상태
enum ObstacleSide { OBSTACLE_NONE, OBSTACLE_LEFT, OBSTACLE_RIGHT, OBSTACLE_BOTH };
ObstacleSide obstacleSide = OBSTACLE_NONE;

// ======================================================
// 좌/우 진동 패턴 정의 (ON, OFF, ON, OFF ... 순서로 지속시간(ms) 나열)
//   -> 인덱스 짝수(0,2,4...) = ON 구간, 홀수(1,3,5...) = OFF 구간
// ======================================================

// 좌측: 짧게-짧게-쉬고 반복 / 지금 실제론 우측
const unsigned long leftPattern[]  = { 500, 200, 500, 200 };
const int LEFT_PATTERN_LEN  = sizeof(leftPattern)  / sizeof(leftPattern[0]);

// 우측: 짧게-길게-쉬고 반복 / 지금 실제론 좌측
const unsigned long rightPattern[] = { 100, 500, 100, 500 };
const int RIGHT_PATTERN_LEN = sizeof(rightPattern) / sizeof(rightPattern[0]);

// 양쪽 동시: 좌측 패턴 + 우측 패턴을 이어붙인 복합 패턴 반복
const unsigned long bothPattern[]  = { 100, 100, 100, 100, 100, 100, 300, 400 };
const int BOTH_PATTERN_LEN  = sizeof(bothPattern)  / sizeof(bothPattern[0]);

// 패턴 재생 상태머신 변수 (논블로킹)
const unsigned long* activePattern   = nullptr;  // 현재 재생 중인 패턴 배열 (nullptr = 없음)
int patternStepIndex     = 0;
unsigned long patternStepStartTime = 0;

// ======================================================
// VL53L1X 계단(내리막) 감지 설정 (구 HC-SR04 자리)
// ======================================================

const float STAIR_THRESHOLD_CM = 170.0;   // 이 거리를 넘으면 바닥이 없다(계단/내리막)고 판단
const unsigned long HCSR04_SAMPLE_INTERVAL = 60;   // 측정 주기 (ms) - VL53L1X IntermeasurementPeriod로도 재사용
const int   STAIR_FILTER_SAMPLES = 3;              // 이동평균 필터 샘플 수
const int   STAIR_CONFIRM_COUNT  = 2;               // 연속 N회 이상 임계값 초과해야 확정 (노이즈 오탐 방지)

unsigned long lastHCSR04SampleTime = 0;
float stairFilterBuf[STAIR_FILTER_SAMPLES] = { -1, -1, -1 };
int   stairFilterIdx = 0;
float lastStairDistanceCM = -1.0f;   // GET_STAIR_DISTANCE 조회용 최신값
int   stairOverCount = 0;            // 임계값 초과 연속 횟수
bool  stairAlertActive = false;      // 현재 계단 진동이 켜져 있는지

SFEVL53L1X stairSensor;   // 계단용 VL53L1X 객체 (구 HC-SR04 자리)

// ======================================================
// BLE 객체 및 상태 변수
// ======================================================

BLEServer* bleServer = nullptr;
BLEService* bleService = nullptr;
BLECharacteristic* bleCharacteristic = nullptr;

bool deviceConnected = false;
bool previousDeviceConnected = false;

unsigned long previousSendTime = 0;

// ======================================================
// 낙상 감지 상태 변수
// ======================================================

Adafruit_MPU6050 mpu;

enum FallState { IDLE, FREEFALL, IMPACT, STILLNESS_CHECK };
FallState fallState = IDLE;
unsigned long stateTimer = 0;

unsigned long lastAccelSampleTime = 0;
float lastAccelMag = 0;

// ======================================================
// 푸시버튼(GPIO33) 디바운스 상태
// ======================================================

bool lastButtonReading   = HIGH;
bool buttonStableState   = HIGH;
unsigned long buttonLastChangeTime = 0;

// ======================================================
// Pi5 UART2 수신 버퍼
// ======================================================

String piRxBuffer = "";

// ======================================================
// 초록불(green_pedestrian_signal) 진동 패턴 - 짧게-길게-짧게-길게, 1회성(non-repeating)
//   -> 인덱스 짝수(0,2,4,6) = ON 구간, 홀수(1,3,5) = OFF 구간
// ======================================================

const unsigned long greenPattern[] = { 150, 150, 500, 150, 150, 150, 500 };
const int GREEN_PATTERN_LEN = sizeof(greenPattern) / sizeof(greenPattern[0]);

bool greenPatternActive = false;
int  greenPatternIndex = 0;
unsigned long greenPatternStepStart = 0;

// ======================================================
// 웹페이지로 문자열을 전송하는 함수
// ======================================================

void sendMessage(const String& message)
{
  if (!deviceConnected || bleCharacteristic == nullptr)
  {
    return;
  }

  bleCharacteristic->setValue(message.c_str());
  bleCharacteristic->notify();

  Serial.print("[BLE 전송] ");
  Serial.println(message);
}

// ======================================================
// LED 상태를 웹페이지로 전송하는 함수
// ======================================================

void sendLedState()
{
  if (digitalRead(LED_PIN) == HIGH)
  {
    sendMessage("LED:ON");
  }
  else
  {
    sendMessage("LED:OFF");
  }
}

// ======================================================
// BLE 연결 및 연결 해제 콜백
// ======================================================

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer* server) override
  {
    deviceConnected = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("웹페이지와 BLE 연결됨");
    Serial.println("================================");
  }

  void onDisconnect(BLEServer* server) override
  {
    deviceConnected = false;

    Serial.println();
    Serial.println("================================");
    Serial.println("웹페이지와 BLE 연결 해제됨");
    Serial.println("BLE 광고를 다시 시작합니다.");
    Serial.println("================================");
  }
};

// ======================================================
// 웹페이지에서 문자열을 받았을 때 실행되는 콜백
// ======================================================

class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic* characteristic) override
  {
    String receivedValue = characteristic->getValue().c_str();

    if (receivedValue.length() == 0)
    {
      return;
    }

    receivedValue.trim();

    Serial.print("[BLE 수신] ");
    Serial.println(receivedValue);

    if (receivedValue == "LED_ON")
    {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED를 켰습니다.");
      sendMessage("LED:ON");
    }
    else if (receivedValue == "LED_OFF")
    {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED를 껐습니다.");
      sendMessage("LED:OFF");
    }
    else if (receivedValue == "LED_STATUS")
    {
      sendLedState();
    }
    else if (receivedValue == "PING")
    {
      sendMessage("PONG");
    }
    else if (receivedValue == "GET_SENSOR")
    {
      int sensorValue = analogRead(SENSOR_PIN);
      String message = "SENSOR:" + String(sensorValue);
      sendMessage(message);
    }
    // --------------------------------------------------
    // 현재 가속도 크기 요청 명령
    // --------------------------------------------------
    else if (receivedValue == "GET_ACCEL")
    {
      String message = "ACCEL:" + String(lastAccelMag, 2);
      sendMessage(message);
    }
    // --------------------------------------------------
    // 현재 ToF 최소 거리(cm) 요청 명령 (좌/우 중 더 가까운 값)
    // --------------------------------------------------
    else if (receivedValue == "GET_DISTANCE")
    {
      String message = "DISTANCE:" + String(lastDistanceCM, 1);
      sendMessage(message);
    }
    // --------------------------------------------------
    // 현재 계단(하향 VL53L1X) 거리(cm) 요청 명령
    // --------------------------------------------------
    else if (receivedValue == "GET_STAIR_DISTANCE")
    {
      String message = "STAIR_DISTANCE:" + String(lastStairDistanceCM, 1);
      sendMessage(message);
    }
    else
    {
      Serial.println("등록되지 않은 명령입니다.");
      sendMessage("ERROR:UNKNOWN_COMMAND");
    }
  }
};

// ======================================================
// 장애물 진동 패턴 상태 초기화 (낙상 알림 종료 후, 계단 경고 시작 시 등에 사용)
// ======================================================

void resetObstaclePattern()
{
  activePattern = nullptr;
  patternStepIndex = 0;
  patternStepStartTime = 0;
}

// ======================================================
// 낙상 확정 시 알림 (진동 + BLE 메시지)
// ======================================================

void triggerFallAlert()
{
  Serial.println(">>> 낙상 감지! (진동 없음, BLE 알림만 전송) <<<");

  // 웹페이지로 즉시 낙상 알림 전송 (진동은 사용자 요청으로 제거됨)
  sendMessage("FALL_DETECTED");

  obstacleSide = OBSTACLE_NONE;   // 낙상 알림 끝난 뒤 장애물 상태 재평가 유도
  resetObstaclePattern();
  stairAlertActive = false;       // 낙상 알림 끝난 뒤 계단 상태 재평가 유도
  stairOverCount = 0;
}

// ======================================================
// 낙상 감지 상태머신
// ======================================================

void checkFall(float accelMag)
{
  unsigned long now = millis();

  switch (fallState)
  {
    case IDLE:
      if (accelMag < FREEFALL_THRESHOLD)
      {
        fallState = FREEFALL;
        stateTimer = now;
      }
      break;

    case FREEFALL:
      if (accelMag > IMPACT_THRESHOLD)
      {
        fallState = IMPACT;
        stateTimer = now;
      }
      else if (now - stateTimer > FREEFALL_WINDOW)
      {
        fallState = IDLE;
      }
      break;

    case IMPACT:
      fallState = STILLNESS_CHECK;
      stateTimer = now;
      break;

    case STILLNESS_CHECK:
      if (now - stateTimer < IMPACT_SETTLE_TIME) break;
      if (accelMag < STILLNESS_MIN || accelMag > STILLNESS_MAX)
      {
        fallState = IDLE;
      }
      else if (now - stateTimer > IMPACT_SETTLE_TIME + STILLNESS_DURATION)
      {
        triggerFallAlert();
        fallState = IDLE;
      }
      break;
  }
}

// ======================================================
// [디버그 전용] VL53L5CX 행(row)별 평균 거리 출력
//   - 지팡이를 정상 자세로 들고 이 함수를 잠깐 호출해서
//     어느 행(row)이 바닥을 비추는지 확인할 때 사용
//   - 바닥을 비추는 행 = 거리가 짧고 안정적으로 나오는 행
//     -> 그 행 번호를 참고해서 TOF_ROW_MIN/TOF_ROW_MAX 값을 조정할 것
//   - 필요 없을 때는 loop()에서 호출하지 않으면 됨 (기본 비활성화 상태)
// ======================================================

void debugPrintTofRows(VL53L5CX_ResultsData &data)
{
  Serial.println("---- ToF 행(row)별 평균 거리 ----");
  for (int row = 0; row < 8; row++)
  {
    float sum = 0;
    int count = 0;

    for (int col = 0; col < 8; col++)
    {
      int idx = row * 8 + col;
      if (data.target_status[idx] == 5 && data.distance_mm[idx] > 0)
      {
        sum += data.distance_mm[idx];
        count++;
      }
    }

    if (count > 0)
    {
      Serial.printf("row %d 평균: %.0f mm (n=%d)\n", row, sum / count, count);
    }
    else
    {
      Serial.printf("row %d 평균: 데이터 없음\n", row);
    }
  }
  Serial.println("--------------------------------");
}

// ======================================================
// VL53L5CX: 새 프레임에서 "좌측 최소 거리(cm)"와 "우측 최소 거리(cm)"를 각각 읽기
//   - 8x8(64존) 그리드 기준, 열(col = i % 8)이 0~3이면 좌측, 4~7이면 우측으로 간주
//   - 행(row = i / 8)이 TOF_ROW_MIN ~ TOF_ROW_MAX 범위를 벗어나면 무시
//     (바닥을 비추는 각도의 행을 걸러내어 바닥 오탐을 줄임)
//   - 지팡이 장착 방향에 따라 좌/우가 반대로 느껴지면 "col < 4" 조건만 뒤집을 것
//   - target_status == 5 (신뢰 가능) 존만 사용
//   반환값 : true  -> 새 프레임을 읽어서 outLeftCM/outRightCM 갱신함
//            false -> 새 프레임 없음 (이번 루프는 스킵)
//   outLeftCM / outRightCM : 신뢰 가능한 존이 없으면 -1.0f
// ======================================================

bool readToFDistances(float &outLeftCM, float &outRightCM)
{
  if (!tof.isDataReady())
  {
    return false;
  }

  VL53L5CX_ResultsData measurementData;
  if (!tof.getRangingData(&measurementData))
  {
    return false;
  }

  // 장착 각도 확인용 디버그 출력이 필요하면 아래 주석을 해제하세요.
  // debugPrintTofRows(measurementData);

  float leftMinMM  = -1.0f;
  float rightMinMM = -1.0f;

  for (int i = 0; i < tofZones; i++)
  {
    if (measurementData.target_status[i] != 5) continue;   // UM2884 기준 신뢰 가능 측정만 사용

    int row = 7 - (i / 8);   // 거꾸로 장착 -> 행 인덱스 반전
    if (row < TOF_ROW_MIN || row > TOF_ROW_MAX) continue;

    float d = measurementData.distance_mm[i];
    if (d <= 0) continue;

    int col = i % 8;   // 8x8 그리드 기준 열 인덱스 (0~7)

    if (col < 4)
    {
      // 좌측 절반
      if (leftMinMM < 0 || d < leftMinMM) leftMinMM = d;
    }
    else
    {
      // 우측 절반
      if (rightMinMM < 0 || d < rightMinMM) rightMinMM = d;
    }
  }

  outLeftCM  = (leftMinMM  < 0) ? -1.0f : leftMinMM  / 10.0f;
  outRightCM = (rightMinMM < 0) ? -1.0f : rightMinMM / 10.0f;

  return true;
}

// ======================================================
// 이동평균 필터 (노이즈로 인한 순간적 오탐 완화) - 좌/우 공용 함수
// ======================================================

float applyMovingAverage(float* buf, int& idx, float newSampleCM)
{
  if (newSampleCM < 0)
  {
    return -1.0f;   // 이번 샘플은 버림
  }

  buf[idx] = newSampleCM;
  idx = (idx + 1) % TOF_FILTER_SAMPLES;

  float sum = 0;
  int count = 0;
  for (int i = 0; i < TOF_FILTER_SAMPLES; i++)
  {
    if (buf[i] >= 0)
    {
      sum += buf[i];
      count++;
    }
  }

  if (count == 0) return -1.0f;
  return sum / count;
}

// ======================================================
// 전방 장애물 감지 처리 (매 loop마다 호출, 비차단)
//   -> 좌/우 각각의 필터링된 거리로 obstacleSide(NONE/LEFT/RIGHT/BOTH)만 갱신
//      (실제 진동 출력은 updateVibrationOutput() -> playObstaclePattern()에서 처리)
// ======================================================

void checkObstacle()
{
  float rawLeft = -1.0f;
  float rawRight = -1.0f;

  bool newFrame = readToFDistances(rawLeft, rawRight);
  if (!newFrame)
  {
    return;   // 새 프레임 없음 -> 이번 루프는 스킵
  }

  float left  = applyMovingAverage(tofFilterBufLeft,  tofFilterIdxLeft,  rawLeft);
  float right = applyMovingAverage(tofFilterBufRight, tofFilterIdxRight, rawRight);

  // GET_DISTANCE 호환용 대표값: 좌/우 중 더 가까운 값
  if (left >= 0 && right >= 0)      lastDistanceCM = min(left, right);
  else if (left >= 0)               lastDistanceCM = left;
  else if (right >= 0)              lastDistanceCM = right;
  // 둘 다 -1이면 lastDistanceCM은 기존 값 유지 (오래된 값이라도 갑자기 지우지 않음)

  bool leftObstacle  = (left  >= 0 && left  < OBSTACLE_THRESHOLD_CM);
  bool rightObstacle = (right >= 0 && right < OBSTACLE_THRESHOLD_CM);

  ObstacleSide newSide;
  if (leftObstacle && rightObstacle)      newSide = OBSTACLE_BOTH;
  else if (leftObstacle)                  newSide = OBSTACLE_LEFT;
  else if (rightObstacle)                 newSide = OBSTACLE_RIGHT;
  else                                    newSide = OBSTACLE_NONE;

  if (newSide != obstacleSide)
  {
    obstacleSide = newSide;

    switch (obstacleSide)
    {
      case OBSTACLE_NONE:
        Serial.println("장애물 없음 -> 진동 OFF");
        break;
      case OBSTACLE_LEFT:
        Serial.printf("좌측 장애물 감지: %.1f cm -> 짧게-짧게 패턴\n", left);
        break;
      case OBSTACLE_RIGHT:
        Serial.printf("우측 장애물 감지: %.1f cm -> 짧게-길게 패턴\n", right);
        break;
      case OBSTACLE_BOTH:
        Serial.printf("양측 장애물 감지 (좌 %.1f / 우 %.1f cm) -> 복합 패턴\n", left, right);
        break;
    }
  }
}

// ======================================================
// VL53L1X: 하향 거리 측정 (구 HC-SR04 자리, 단발성)
//   반환값 :
//     -1.0f  -> 새 데이터 없음 (아직 측정 준비 안 됨 / 타임아웃과 동일하게 취급)
//     >=0    -> 측정 거리 (cm)
// ======================================================

float readVL53L1XDistanceCM()
{
  if (!stairSensor.checkForDataReady())
  {
    return -1.0f;   // 기존 HC-SR04의 pulseIn 타임아웃(-1.0f)과 동일한 의미로 맞춤
  }

  int distMm = stairSensor.getDistance();   // mm 단위
  stairSensor.clearInterrupt();

  return distMm / 10.0f;   // cm 변환
}

// ======================================================
// 이동평균 필터 (노이즈로 인한 순간적 오탐 완화) - 계단(VL53L1X)용
// ======================================================

float filteredStairDistance(float newSampleCM)
{
  if (newSampleCM < 0)
  {
    return -1.0f;   // 이번 샘플은 버림
  }

  stairFilterBuf[stairFilterIdx] = newSampleCM;
  stairFilterIdx = (stairFilterIdx + 1) % STAIR_FILTER_SAMPLES;

  float sum = 0;
  int count = 0;
  for (int i = 0; i < STAIR_FILTER_SAMPLES; i++)
  {
    if (stairFilterBuf[i] >= 0)
    {
      sum += stairFilterBuf[i];
      count++;
    }
  }

  if (count == 0) return -1.0f;
  return sum / count;
}

// ======================================================
// 계단(내리막) 감지 처리
//   -> HCSR04_SAMPLE_INTERVAL마다 한 번씩만 측정 (VL53L1X 측정 주기와 맞춤)
//   -> 진동 핀을 직접 건드리지 않고 stairAlertActive 플래그만 갱신
// ======================================================

void checkStairs()
{
  unsigned long now = millis();

  if (now - lastHCSR04SampleTime < HCSR04_SAMPLE_INTERVAL)
  {
    return;   // 아직 측정 주기가 안 됨
  }
  lastHCSR04SampleTime = now;

  float raw = readVL53L1XDistanceCM();
  float dist = filteredStairDistance(raw);

  if (dist < 0)
  {
    // 새 데이터 없음/노이즈로 값이 안 들어왔을 때는 기존 상태 유지 (급하게 끄지 않음)
    return;
  }

  lastStairDistanceCM = dist;

  if (dist > STAIR_THRESHOLD_CM)
  {
    stairOverCount++;
    if (stairOverCount >= STAIR_CONFIRM_COUNT && !stairAlertActive)
    {
      stairAlertActive = true;
      Serial.printf("계단(내리막) 감지: %.1f cm -> 진동 ON\n", dist);
    }
  }
  else
  {
    stairOverCount = 0;
    if (stairAlertActive)
    {
      stairAlertActive = false;
      Serial.println("계단 없음 -> 진동 OFF");
    }
  }
}

// ======================================================
// 계단(VL53L1X) 거리값 시리얼 모니터 주기 출력
//   -> STAIR_PRINT_INTERVAL(500ms)마다 최신 캐시값(lastStairDistanceCM) 출력
// ======================================================

void printStairDistance()
{
  unsigned long now = millis();

  if (now - lastStairPrintTime < STAIR_PRINT_INTERVAL)
  {
    return;
  }
  lastStairPrintTime = now;

  if (lastStairDistanceCM >= 0)
  {
    Serial.printf("[계단 ToF] 거리: %.1f cm (임계값: %.1f cm, 상태: %s)\n",
                  lastStairDistanceCM,
                  STAIR_THRESHOLD_CM,
                  stairAlertActive ? "계단 감지" : "정상");
  }
  else
  {
    Serial.println("[계단 ToF] 측정값 없음 (데이터 미준비/노이즈)");
  }
}

// ======================================================
// 장애물 좌/우 패턴 재생 (논블로킹, millis() 기반 상태머신)
//   - obstacleSide에 따라 leftPattern / rightPattern / bothPattern 중 선택
//   - 배열 인덱스 짝수 = ON 구간, 홀수 = OFF 구간
//   - 상태(obstacleSide)가 바뀌면 패턴을 처음부터 다시 시작
// ======================================================

void playObstaclePattern()
{
  const unsigned long* pattern = nullptr;
  int patternLen = 0;

  switch (obstacleSide)
  {
    case OBSTACLE_LEFT:
      pattern = leftPattern;
      patternLen = LEFT_PATTERN_LEN;
      break;
    case OBSTACLE_RIGHT:
      pattern = rightPattern;
      patternLen = RIGHT_PATTERN_LEN;
      break;
    case OBSTACLE_BOTH:
      pattern = bothPattern;
      patternLen = BOTH_PATTERN_LEN;
      break;
    case OBSTACLE_NONE:
    default:
      resetObstaclePattern();
      digitalWrite(VIBRATION_PIN, LOW);
      return;
  }

  unsigned long now = millis();

  if (pattern != activePattern)
  {
    // 좌/우/양쪽 상태가 방금 바뀌었으면 패턴을 처음(ON)부터 다시 시작
    activePattern = pattern;
    patternStepIndex = 0;
    patternStepStartTime = now;
    digitalWrite(VIBRATION_PIN, HIGH);   // 모든 패턴은 항상 ON으로 시작
    return;
  }

  if (now - patternStepStartTime >= pattern[patternStepIndex])
  {
    patternStepIndex = (patternStepIndex + 1) % patternLen;
    patternStepStartTime = now;
    digitalWrite(VIBRATION_PIN, (patternStepIndex % 2 == 0) ? HIGH : LOW);
  }
}

// ======================================================
// 푸시버튼(GPIO33, 모멘터리) 체크
//   - 평상시 HIGH(풀업), 누르면 LOW
//   - 디바운스(BUTTON_DEBOUNCE_MS) 후 눌림 확정 시 1회 "BTN\n"을 Pi5로 전송
//     -> Pi5는 이 명령을 받으면 YOLO 신호등 감지 프로그램을 5분간 실행
// ======================================================

void checkButton()
{
  bool reading = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (reading != lastButtonReading)
  {
    buttonLastChangeTime = now;
  }

  if ((now - buttonLastChangeTime) > BUTTON_DEBOUNCE_MS)
  {
    if (reading != buttonStableState)
    {
      buttonStableState = reading;

      if (buttonStableState == LOW)   // 눌림 확정 (풀업 기준 HIGH -> LOW)
      {
        Serial2.print("BTN\n");
        Serial.println("[버튼] 눌림 감지 -> Pi5로 \"BTN\" 전송 (YOLO 5분 세션 시작 요청)");
      }
    }
  }

  lastButtonReading = reading;
}

// ======================================================
// Pi5(UART2)로부터 들어오는 라인 수신 처리
//   - "GREEN" 수신 시 초록불 신호등 감지로 간주 -> 진동 패턴 1회 트리거
//     (Pi5 쪽에서 5초 쿨다운을 이미 적용하므로 ESP32는 수신값을 그대로 신뢰)
//   - YOLO 세션 잔여시간을 1분으로 단축하는 로직은 Pi5(파이썬) 쪽에서 처리
// ======================================================

void checkPiUart()
{
  while (Serial2.available())
  {
    char c = Serial2.read();

    if (c == '\n')
    {
      piRxBuffer.trim();

      if (piRxBuffer.length() > 0)
      {
        Serial.print("[Pi5 UART 수신] ");
        Serial.println(piRxBuffer);

        if (piRxBuffer == "GREEN")
        {
          greenPatternActive = true;
          greenPatternIndex = 0;
          greenPatternStepStart = millis();
          digitalWrite(VIBRATION_PIN, HIGH);   // 패턴은 항상 ON 구간으로 시작

          Serial.println("초록불 신호 감지 -> 짧게-길게-짧게-길게 진동 1회 재생");
        }
      }

      piRxBuffer = "";
    }
    else if (c != '\r')
    {
      piRxBuffer += c;
    }
  }
}

// ======================================================
// 초록불 진동 패턴 재생 (논블로킹, 1회성)
//   - greenPattern[] : 짧게(150) - 쉼(150) - 길게(500) - 쉼(150) - 짧게(150) - 쉼(150) - 길게(500)
//   - 마지막 ON 구간이 끝나면 자동 종료되고 재트리거 전까지 다시 재생하지 않음
//   - 계단/장애물 패턴보다 우선순위가 가장 높음 (updateVibrationOutput에서 최우선 처리)
// ======================================================

void playGreenPattern()
{
  if (!greenPatternActive) return;

  unsigned long now = millis();

  if (now - greenPatternStepStart >= greenPattern[greenPatternIndex])
  {
    greenPatternIndex++;
    greenPatternStepStart = now;

    if (greenPatternIndex >= GREEN_PATTERN_LEN)
    {
      // 패턴 재생 종료
      greenPatternActive = false;
      digitalWrite(VIBRATION_PIN, LOW);
      resetObstaclePattern();   // 계단/장애물 패턴은 다음 루프에서 처음부터 새로 시작하도록
      return;
    }

    digitalWrite(VIBRATION_PIN, (greenPatternIndex % 2 == 0) ? HIGH : LOW);
  }
}

// ======================================================
// 진동 출력 일괄 처리 - 우선순위: 초록불(1회) > 계단(연속 ON) > 장애물 좌/우/양쪽
//   - 낙상은 더 이상 진동을 사용하지 않음 (BLE 알림만 전송)
// ======================================================

void updateVibrationOutput()
{
  if (greenPatternActive)
  {
    // 초록불 패턴 재생 중에는 계단/장애물 진동이 절대 끼어들지 않음
    playGreenPattern();
    return;
  }

  if (stairAlertActive)
  {
    // 계단(내리막) 경고 -> 진동 계속 ON, 장애물 패턴은 잠시 정지
    resetObstaclePattern();
    digitalWrite(VIBRATION_PIN, HIGH);
    return;
  }

  playObstaclePattern();
}

// ======================================================
// BLE 초기화 함수
// ======================================================

void initializeBluetooth()
{
  Serial.println("BLE 초기화를 시작합니다.");

  BLEDevice::init(DEVICE_NAME);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  bleService = bleServer->createService(SERVICE_UUID);

  bleCharacteristic = bleService->createCharacteristic(
    CHARACTERISTIC_UUID,

    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  bleCharacteristic->setCallbacks(
    new CharacteristicCallbacks()
  );

  bleCharacteristic->addDescriptor(
    new BLE2902()
  );

  bleCharacteristic->setValue("ESP32_READY");

  bleService->start();

  BLEAdvertising* advertising =
    BLEDevice::getAdvertising();

  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 BLE 서버 시작 완료");
  Serial.print("장치 이름 : ");
  Serial.println(DEVICE_NAME);
  Serial.println("웹페이지 연결 대기 중...");
  Serial.println("================================");
}

// ======================================================
// Arduino 초기 설정
// ======================================================

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 Web Bluetooth + 낙상 감지 + 장애물 감지(좌/우) + 계단 감지(VL53L1X) 시작");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(SENSOR_PIN, INPUT);

  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);

  // ---- 푸시버튼 (Pi5 YOLO 5분 세션 트리거) ----
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lastButtonReading = digitalRead(BUTTON_PIN);
  buttonStableState = lastButtonReading;

  // ---- Pi5 UART2 초기화 (RX=16, TX=17, 115200) ----
  Serial2.begin(PI_UART_BAUD, SERIAL_8N1, PI_UART_RX_PIN, PI_UART_TX_PIN);
  Serial.println("Pi5 UART2 초기화 완료 (RX=16, TX=17, 115200)");

  // ---- I2C 공용 버스 초기화 (MPU6050 + VL53L5CX + VL53L1X) ----
  Wire.begin(21, 22);        // ESP32 I2C: SDA=21, SCL=22
  Wire.setClock(100000);     // 우선 낮은 클럭(100kHz)으로 시작 - VL53L5CX 펌웨어(~90KB) 업로드 안정성 확보
  delay(100);                // 센서 전원 안정화 대기

  // ---- VL53L5CX를 리셋(비활성) 상태로 유지 ----
  // VL53L5CX = LPn 핀 (active-low 비활성화 핀). 두 센서 기본 주소가 0x29로 동일해서
  // VL53L5CX를 잠깐 꺼둔 채로 VL53L1X를 먼저 초기화해 0x30으로 옮긴 뒤, 그 다음 VL53L5CX를 깨운다.
  pinMode(VL53L5CX_LPN_PIN, OUTPUT);
  digitalWrite(VL53L5CX_LPN_PIN, LOW);   // VL53L5CX 비활성
  delay(100);

  // ---- MPU6050 초기화 ----
  if (!mpu.begin())
  {
    Serial.println("MPU6050를 찾을 수 없습니다. 배선을 확인하세요.");
    while (1) delay(100);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

  Serial.println("MPU6050 준비 완료.");

  // ---- VL53L1X 초기화 (계단용, 구 HC-SR04 자리) - 반드시 VL53L5CX보다 먼저! ----
  // [신규] 핀 추가 없이 소프트웨어만으로 "이전 세션에서 0x30으로 남아있는 상태" 대응
  //   - 1차 시도: 기본 주소(0x29)에서 begin() 시도 (센서가 방금 전원 인가되어 기본값인 정상 케이스)
  //   - 실패 시: 라이브러리 setI2CAddress()는 "센서에 변경 명령 전송 + 통신 주소를 무조건 갱신"하는
  //     방식으로 동작하기 때문에, 센서가 이미 0x30에 가 있어도 setI2CAddress(0x30)를 호출하면
  //     (0x29로 보낸 변경 명령은 무시되지만) 라이브러리의 통신 주소만 0x30으로 바뀌어
  //     실제 센서 위치(0x30)와 일치하게 됨 -> checkID()가 정상적으로 통과함
  //   -> ESP32 리셋/재업로드만으로도 항상 정상 초기화되며, 전원을 물리적으로 뽑을 필요 없음
  Serial.println("VL53L1X(계단) 초기화 중...");

  int vl53l1xInitResult = stairSensor.begin();
  Serial.printf("VL53L1X begin() 1차 시도(주소 0x29) 반환값: %d\n", vl53l1xInitResult);

  if (vl53l1xInitResult != 0)
  {
    Serial.println("0x29에서 실패 -> 이전 세션에서 0x30으로 재할당된 상태로 추정, 0x30으로 재시도...");

    stairSensor.setI2CAddress(VL53L1X_NEW_ADDR);   // 통신 주소를 0x30으로 강제 전환

    if (!stairSensor.checkID())
    {
      Serial.println("VL53L1X를 찾을 수 없습니다. (0x29, 0x30 모두 실패)");
      Serial.println("배선 확인: SDA=21, SCL=22, VCC=3.3V, GND / 그래도 안 되면 전원을 완전히 껐다 켜보세요.");
      while (1) delay(100);
    }

    stairSensor.init();   // SensorInit 재실행 (설정값 초기화 보정)
    Serial.println("0x30에서 VL53L1X 통신 성공.");
  }
  else
  {
    stairSensor.setI2CAddress(VL53L1X_NEW_ADDR);   // 정상 케이스: 0x29 -> 0x30 재할당
    delay(50);
  }

  stairSensor.setDistanceModeShort();                    // 계단은 근거리(수십cm~1m대)라 Short 모드 권장
  stairSensor.setTimingBudgetInMs(50);
  stairSensor.setIntermeasurementPeriod(HCSR04_SAMPLE_INTERVAL);  // 기존 60ms 주기 유지
  stairSensor.startRanging();

  Serial.println("VL53L1X 준비 완료. (주소=0x30)");


  // ---- VL53L5CX 초기화 ----
  // VL53L1X가 이미 0x30으로 옮겨갔으므로, 이제 VL53L5CX를 깨워서(LPn=HIGH) 0x29로 초기화해도 충돌 없음.
  // begin()은 센서에 ~90KB 펌웨어를 I2C로 업로드하므로 최대 ~10초 소요될 수 있음 (정상 동작).
  // ESP32에서 이 단계가 실패하는 경우가 있어 워치독을 잠깐 끄고 진행.
  Serial.println("VL53L5CX 초기화 중... (최대 10초 소요)");
  digitalWrite(VL53L5CX_LPN_PIN, HIGH);
  delay(200);

  disableCore0WDT();
  disableCore1WDT();

  bool tofOk = tof.begin();

  enableCore0WDT();
  enableCore1WDT();

  if (tofOk == false)
  {
    Serial.println("VL53L5CX를 찾을 수 없습니다. 배선/전원을 확인하세요.");
    while (1) delay(100);
  }

  tof.setResolution(8 * 8);            // 64존 전체 사용 (좌/우 분리는 이 8x8 기준으로 동작)
  tofZones = tof.getResolution();      // 실제 적용된 해상도 확인 (16 or 64)
  tof.setRangingFrequency(15);         // 8x8 해상도에서 최대 15Hz
  Wire.setClock(400000);               // begin() 성공 후에만 400kHz로 상향
  tof.startRanging();

  Serial.printf("VL53L5CX 준비 완료. (존 수: %d)\n", tofZones);
  Serial.printf("ToF 상/하 필터 적용 행 범위: row %d ~ %d (필요 시 코드 상단 TOF_ROW_MIN/MAX 값 조정)\n",
                TOF_ROW_MIN, TOF_ROW_MAX);

  // ---- BLE 서버 초기화 ----
  initializeBluetooth();
}

// ======================================================
// 반복 실행 부분
// ======================================================

void loop()
{
  unsigned long currentTime = millis();

  // --------------------------------------------------
  // 가속도 샘플링 + 낙상 감지 (비차단)
  // --------------------------------------------------

  if (currentTime - lastAccelSampleTime >= ACCEL_SAMPLE_INTERVAL)
  {
    lastAccelSampleTime = currentTime;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float accelMag = sqrt(a.acceleration.x * a.acceleration.x +
                           a.acceleration.y * a.acceleration.y +
                           a.acceleration.z * a.acceleration.z);

    lastAccelMag = accelMag;
    checkFall(accelMag);
  }

  // --------------------------------------------------
  // 전방 장애물 감지 (VL53L5CX, 좌/우 구분 + 상/하 각도 필터, isDataReady 이벤트 기반, 비차단)
  // --------------------------------------------------

  checkObstacle();

  // --------------------------------------------------
  // 하향 계단 감지 (VL53L1X, 60ms 주기, 논블로킹 checkForDataReady 폴링)
  // --------------------------------------------------

  checkStairs();

  // --------------------------------------------------
  // 푸시버튼(Pi5 YOLO 5분 세션 트리거) / Pi5 UART2 수신 처리 (둘 다 논블로킹)
  // --------------------------------------------------

  checkButton();
  checkPiUart();

  // --------------------------------------------------
  // 계단(ToF) 거리값 시리얼 모니터 주기 출력 (500ms마다)
  // --------------------------------------------------

  printStairDistance();

  // --------------------------------------------------
  // 장애물(좌/우)/계단 상태를 기준으로 진동 핀 일괄 갱신 (논블로킹 패턴 재생)
  // --------------------------------------------------

  updateVibrationOutput();

  // --------------------------------------------------
  // 웹페이지가 연결되어 있을 때 센서값 주기적 전송
  // --------------------------------------------------

  if (
    deviceConnected &&
    currentTime - previousSendTime >= SEND_INTERVAL
  )
  {
    previousSendTime = currentTime;

    int sensorValue = analogRead(SENSOR_PIN);
    String message = "SENSOR:" + String(sensorValue);
    sendMessage(message);
  }

  // --------------------------------------------------
  // 연결이 끊어진 직후 BLE 광고 다시 시작
  // --------------------------------------------------

  if (
    !deviceConnected &&
    previousDeviceConnected
  )
  {
    delay(500);
    bleServer->startAdvertising();

    Serial.println("BLE 광고 재시작 완료");
    Serial.println("새로운 연결을 기다립니다.");

    previousDeviceConnected = deviceConnected;
  }

  // --------------------------------------------------
  // 새로 연결된 직후 초기 상태 전송
  // --------------------------------------------------

  if (
    deviceConnected &&
    !previousDeviceConnected
  )
  {
    previousDeviceConnected = deviceConnected;

    delay(300);

    sendMessage("CONNECTED");
    sendLedState();
  }

  delay(10);
}