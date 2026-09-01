/*
  프로젝트명 : ESP32 Web Bluetooth + 낙상 감지(MPU6050) + 전방 장애물 감지(VL53L5CX, 좌/우 구분) + 계단 감지(VL53L1X)
              + Pi5 UART2 연동(YOLO 신호등 감지 트리거/결과 수신) + YOLO 세션 표시 LED
  설     명 : Netlify 웹페이지와 ESP32를 BLE로 연결하고,
              MPU6050 가속도센서로 낙상을, VL53L5CX ToF로 전방 장애물(좌/우 구분)을,
              VL53L1X ToF로 하향 계단(내리막)을 감지.
              추가로 UART2(GPIO16/17)로 Pi5와 통신하여 YOLO 신호등 감지 세션을 트리거하고
              초록불 감지 결과를 받아 진동으로 알림.
              Pi5가 YOLO 세션을 시작/종료할 때 "YOLO_ON"/"YOLO_OFF"를 보내면
              GPIO27에 연결된 LED로 세션 진행 여부를 시각적으로 표시.

  통신 방식 : Bluetooth Low Energy (웹페이지) + UART2 115200bps (Pi5)
  장치 이름 : NAVCANE_ESP32

  [FINAL v8 핵심 - 재결합 순간의 0x29 충돌 자체를 예방]
    - VL53L1X 분리 감지 즉시 VL53L5CX LPn(GPIO25)을 LOW로 유지.
    - 계단 센서가 빠져 있는 동안 VL53L5CX I2C 접근/장애물 감지는 일시 중지.
    - VL53L1X 재결합 시 0x29에서 단독으로 소프트리셋/초기화 후 0x30으로 재할당.
    - VL53L1X가 0x30에 정상 복구된 것을 확인한 뒤에만 VL53L5CX LPn HIGH.
    - XSHUT/별도 삽입 감지 핀 없이 hot-plug 시 0x29 충돌을 예방하기 위한 안전 우선 방식.

  [이번 수정 내용 - 장애물 감지용 좌/우 진동모터 분리 + 거리 기반 가변 속도]
    - 기존에는 장애물(좌/우/양쪽)도 낙상/계단/초록불과 같은 메인 진동모터(GPIO32)를
      패턴(leftPattern/rightPattern/bothPattern)으로 공유해서 울렸음.
    - 이제 진동모터가 3개로 늘어났다고 가정하고, 장애물 감지 전용 모터 2개를 새로 분리:
        - LEFT_VIBRATION_PIN  (GPIO14) : 좌측 장애물 전용 진동모터
        - RIGHT_VIBRATION_PIN (GPIO26) : 우측 장애물 전용 진동모터
      -> 왼쪽 구역에서 장애물이 감지되면 좌측 모터만, 오른쪽 구역에서 감지되면
         우측 모터만 울림 (각 1개씩, 서로 독립적으로 동작).
      ※ GPIO14, GPIO26은 현재 코드에서 다른 용도로 쓰이지 않는 핀이라 임의로 선택했습니다.
         실제 배선이 다르면 아래 두 #define 값만 원하는 핀 번호로 바꾸면 됩니다.
    - 기존 메인 진동모터(GPIO32, VIBRATION_PIN)는 낙상(패턴 없이 BLE 알림만)/계단(연속 ON)/
      초록불(1회성 패턴) 전용으로 그대로 유지. 장애물과는 다른 하드웨어이므로 서로 방해하지 않고
      동시에 동작할 수 있음 (단, 낙상 감지 시에는 안전을 위해 장애물 모터도 함께 끔).
    - 거리 기반 가변 속도: 각 방향의 장애물 진동은 OBSTACLE_THRESHOLD_CM(50cm)부터 울리기
      시작하며, 장애물이 가까워질수록 점점 빠르게 깜빡이도록(진동/정지 주기가 짧아지도록) 처리.
        - 50cm 부근  -> OBSTACLE_MAX_INTERVAL_MS(500ms) 주기로 느리게 on/off
        - 0cm(접촉)  -> OBSTACLE_MIN_INTERVAL_MS(60ms)  주기로 빠르게 on/off
      선형 보간(linear interpolation)으로 거리에 따라 주기를 계산하고, millis() 기반
      비블로킹 토글로 구현했기 때문에 loop() 다른 처리들을 막지 않음.

  [이번 추가 수정 - 분절형 포고핀 Hot-plug 자동 복구]
     - FINAL_v2: 0x29/0x30 ACK만으로 VL53L1X 존재를 단정하지 않고,
       ST 식별 레지스터 0x010F~0x0111(Model ID 0xEA / Module Type 0xCC)를 직접 확인한 뒤에만
       VL53L1X로 인정. 다른 장치/잔여 ACK를 VL53L1X로 오인하는 문제 방지.
     - ID가 확인되지 않은 실패에서는 VL53L5CX LPn을 반드시 다시 HIGH로 복귀.
       실제 VL53L1X가 0x29에 있다고 ID까지 확인된 상태에서 초기화가 실패한 경우에만
       주소 충돌 방지를 위해 VL53L5CX를 일시 격리한 채 재시도.
     - MPU6050 / VL53L1X가 빠져도 ESP32 전체를 while(1)로 정지시키지 않음
     - MPU6050 재결합 시 begin() + 측정 범위/필터 자동 재설정
     - VL53L1X 재결합 시 VL53L5CX LPn(GPIO25)을 잠시 LOW로 만들어 0x29 충돌 제거
       후 VL53L1X를 0x29 -> 0x30으로 재할당하고 ranging 자동 재시작
     - VL53L5CX LPn은 I2C 통신만 잠시 차단하는 용도로 사용. 재결합 시 0x29 충돌을 먼저 제거한 뒤
       통신을 복구하고, 프레임이 돌아오지 않을 때만 stop/start -> begin() 전체 재초기화를 fallback으로 수행
     - I2C bus recovery는 단순 센서 미연결에는 실행하지 않고, SDA가 실제 LOW에 고착됐을 때만
       SCL 9펄스 + STOP 조건으로 복구 시도
     - BLE LED 핀은 최종 배선에 맞춰 GPIO2로 고정
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

// BLE 명령용 LED - 최종 배선에 맞춰 GPIO2로 고정
#define LED_PIN 2

// 센서를 연결할 아날로그 입력 핀 (기존 예제용, 필요 없으면 무시)
const int SENSOR_PIN = 34;

// 메인 진동 모터 (트랜지스터/MOSFET 경유) - 낙상/계단/초록불 전용
#define VIBRATION_PIN 32

// 장애물(좌/우) 전용 진동모터 - 신규 분리
#define LEFT_VIBRATION_PIN  14   // 좌측 장애물 전용 진동모터
#define RIGHT_VIBRATION_PIN 26   // 우측 장애물 전용 진동모터

// VL53L5CX (전방 장착, 좌/우 장애물 감지용)
#define VL53L5CX_LPN_PIN 25       // 부팅 시 순차 초기화용 - active-low 비활성화 핀 (기본 주소 0x29 그대로 사용)

// VL53L1X (하향 장착, 계단 감지용 - 구 HC-SR04 자리)
#define VL53L1X_NEW_ADDR         0x60  // SparkFun 라이브러리용 8비트 주소값 -> 실제 7비트 0x30
#define VL53L1X_DEFAULT_ADDR_7   0x29  // 전원 재인가 후 기본 7비트 주소
#define VL53L1X_NEW_ADDR_7       0x30  // 평상시 실제 7비트 주소
#define MPU6050_ADDR_7           0x68
// ※ XSHUT은 3.3V 하드와이어 그대로 사용. Hot-plug 복구는 VL53L5CX LPn(GPIO25)로 주소 충돌을 피해서 수행.

// YOLO 세션 진행 표시 LED (Pi5가 "YOLO_ON"/"YOLO_OFF" 보내면 켜짐/꺼짐)
#define YOLO_LED_PIN 27

// MPU6050, VL53L5CX, VL53L1X 모두 I2C 기본 핀 공유 (SDA=21, SCL=22)
// MPU6050 = 0x68, VL53L5CX = 0x29, VL53L1X = 0x30(재할당 후) -> 주소 충돌 없음

// ------------------------------------------------------
// 라즈베리파이5 UART2 통신 (Pi5의 YOLO 신호등 감지 프로그램 트리거/결과 수신용)
// ------------------------------------------------------
#define PI_UART_BAUD 115200
#define PI_UART_RX_PIN 16   // ESP32 RX2 <- Pi5 TX
#define PI_UART_TX_PIN 17   // ESP32 TX2 -> Pi5 RX

// 푸시버튼 (모멘터리, 눌렸다 떨어지는 버튼) - Pi5 YOLO 5분 세션 시작 트리거
#define BUTTON_PIN 33
const unsigned long BUTTON_DEBOUNCE_MS = 50;

// 센서 데이터 전송 간격
const unsigned long SEND_INTERVAL = 1000;

// ======================================================
// 시리얼 모니터 출력 주기 설정
// ======================================================

const unsigned long STAIR_PRINT_INTERVAL = 20000;
unsigned long lastStairPrintTime = 0;

// ======================================================
// 낙상 감지 임계값
// ======================================================

const float FREEFALL_THRESHOLD = 5.0;
const float IMPACT_THRESHOLD   = 10.0;
const float STILLNESS_MIN      = 7.0;
const float STILLNESS_MAX      = 12.5;

const unsigned long FREEFALL_WINDOW    = 600;
const unsigned long IMPACT_SETTLE_TIME = 500;
const unsigned long STILLNESS_DURATION = 1000;

const unsigned long ACCEL_SAMPLE_INTERVAL = 15;

// ======================================================
// ToF 전방 장애물 감지 설정 (좌/우 구분)
// ======================================================

const float OBSTACLE_THRESHOLD_CM = 50.0;
const int   TOF_FILTER_SAMPLES    = 3;

int TOF_ROW_MIN = 0;
int TOF_ROW_MAX = 2;

SparkFun_VL53L5CX tof;
uint8_t tofZones = 0;
bool tofReady = false;

float tofFilterBufLeft[TOF_FILTER_SAMPLES]  = { -1, -1, -1 };
int   tofFilterIdxLeft  = 0;
float tofFilterBufRight[TOF_FILTER_SAMPLES] = { -1, -1, -1 };
int   tofFilterIdxRight = 0;

float lastDistanceCM = -1.0f;

enum ObstacleSide { OBSTACLE_NONE, OBSTACLE_LEFT, OBSTACLE_RIGHT, OBSTACLE_BOTH };
ObstacleSide obstacleSide = OBSTACLE_NONE;

// ======================================================
// 좌/우 장애물 진동모터 - 거리 기반 가변 속도 설정
// ======================================================

// 50cm(감지 시작)에서 느리게, 0cm(근접)에 가까워질수록 빠르게 on/off
const unsigned long OBSTACLE_MAX_INTERVAL_MS = 500;  // 임계값(50cm) 부근 - 가장 느린 주기
const unsigned long OBSTACLE_MIN_INTERVAL_MS = 60;   // 매우 가까운 거리 - 가장 빠른 주기

bool leftObstacleActive  = false;
bool rightObstacleActive = false;
float leftObstacleDistanceCM  = -1.0f;
float rightObstacleDistanceCM = -1.0f;

bool leftMotorPinState  = false;
unsigned long leftMotorLastToggleTime = 0;

bool rightMotorPinState = false;
unsigned long rightMotorLastToggleTime = 0;

// ======================================================
// VL53L1X 계단(내리막) 감지 설정
// ======================================================

const float STAIR_THRESHOLD_CM = 170.0;
const unsigned long HCSR04_SAMPLE_INTERVAL = 60;
const int   STAIR_FILTER_SAMPLES = 3;
const int   STAIR_CONFIRM_COUNT  = 2;

unsigned long lastHCSR04SampleTime = 0;
float stairFilterBuf[STAIR_FILTER_SAMPLES] = { -1, -1, -1 };
int   stairFilterIdx = 0;
float lastStairDistanceCM = -1.0f;
int   stairOverCount = 0;
bool  stairAlertActive = false;

SFEVL53L1X* stairSensor = nullptr;
bool stairSensorReady = false;

// ======================================================
// 분절형 포고핀 Hot-plug 감시/복구 설정
// ======================================================

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

const unsigned long SENSOR_HEALTH_CHECK_INTERVAL_MS = 500;
const unsigned long STAIR_RECOVERY_RETRY_INTERVAL_MS = 7000;
const unsigned long STAIR_RECONNECT_SETTLE_MS = 3000;       // 재결합 후 최소 안정화 대기
const unsigned long STAIR_ADDR_SAMPLE_INTERVAL_MS = 400;   // 주소 관찰 간격
const uint8_t STAIR_ADDR_STABLE_REQUIRED = 5;              // 같은 주소 5회 연속 확인
const unsigned long TOF_NO_FRAME_TIMEOUT_MS = 2000;
const unsigned long TOF_RECOVERY_RETRY_INTERVAL_MS = 3000;
const uint8_t SENSOR_MISS_LIMIT = 2;

unsigned long lastSensorHealthCheckTime = 0;
unsigned long lastStairRecoveryAttemptTime = 0;
unsigned long stairDisconnectedAtTime = 0;
unsigned long stairLastAddressSampleTime = 0;
unsigned long stairRecoveryCooldownUntil = 0;
uint8_t stairStable29Count = 0;
uint8_t stairStable30Count = 0;
unsigned long lastTofRecoveryAttemptTime = 0;
unsigned long lastTofFrameTime = 0;

uint8_t mpuMissCount = 0;
uint8_t stairMissCount = 0;
uint8_t tofMissCount = 0;

// true면 VL53L5CX LPn을 LOW로 유지하여 0x29에서 I2C 충돌이 나지 않게 격리한 상태
bool tofCommsIsolated = false;

enum StairInitResult
{
  STAIR_INIT_ABSENT,
  STAIR_INIT_OK,
  // VL53L1X ID가 실제 0x29에서 확인됐지만 초기화/주소변경에 실패한 상태.
  // 이 경우에만 VL53L5CX를 HIGH로 올리면 0x29 주소 충돌이 생길 수 있다.
  STAIR_INIT_VERIFIED_AT_29_BUT_FAILED,
  // VL53L1X는 확인됐거나 초기화 시도가 필요했지만 현재 0x29 충돌은 확인되지 않은 실패.
  // VL53L5CX는 다시 HIGH로 복귀시켜도 된다.
  STAIR_INIT_FAILED_NO_CONFLICT
};

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
bool mpuReady = false;

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
// 초록불 진동 패턴
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
// BLE 연결 콜백
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
    else if (receivedValue == "GET_ACCEL")
    {
      String message = "ACCEL:" + String(lastAccelMag, 2);
      sendMessage(message);
    }
    else if (receivedValue == "GET_DISTANCE")
    {
      String message = "DISTANCE:" + String(lastDistanceCM, 1);
      sendMessage(message);
    }
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
// 좌/우 장애물 진동모터 제어
// ======================================================

// 거리(cm) -> 진동 on/off 주기(ms) 계산
// 50cm(임계값) 부근 = 느림(OBSTACLE_MAX_INTERVAL_MS), 0cm(근접) = 빠름(OBSTACLE_MIN_INTERVAL_MS)
unsigned long calcObstacleInterval(float distanceCM)
{
  float d = distanceCM;

  if (d < 0)                      d = OBSTACLE_THRESHOLD_CM;
  if (d > OBSTACLE_THRESHOLD_CM)   d = OBSTACLE_THRESHOLD_CM;

  float ratio = d / OBSTACLE_THRESHOLD_CM;  // 1.0(멀음/느림) ~ 0.0(가까움/빠름)

  unsigned long interval = OBSTACLE_MIN_INTERVAL_MS +
      (unsigned long)(ratio * (float)(OBSTACLE_MAX_INTERVAL_MS - OBSTACLE_MIN_INTERVAL_MS));

  return interval;
}

// 좌/우 모터 공용 - 비블로킹 토글(깜빡임) 처리
void updateSingleObstacleMotor(int pin, bool &pinState, unsigned long &lastToggleTime,
                                bool active, float distanceCM)
{
  if (!active)
  {
    if (pinState)
    {
      pinState = false;
      digitalWrite(pin, LOW);
    }
    return;
  }

  unsigned long interval  = calcObstacleInterval(distanceCM);
  unsigned long halfPeriod = interval / 2;
  unsigned long now = millis();

  if (now - lastToggleTime >= halfPeriod)
  {
    lastToggleTime = now;
    pinState = !pinState;
    digitalWrite(pin, pinState ? HIGH : LOW);
  }
}

// 매 loop()마다 호출 - 좌/우 모터를 각각 독립적으로 갱신
void updateObstacleMotors()
{
  updateSingleObstacleMotor(LEFT_VIBRATION_PIN,  leftMotorPinState,  leftMotorLastToggleTime,
                             leftObstacleActive,  leftObstacleDistanceCM);

  updateSingleObstacleMotor(RIGHT_VIBRATION_PIN, rightMotorPinState, rightMotorLastToggleTime,
                             rightObstacleActive, rightObstacleDistanceCM);
}

// 좌/우 장애물 모터 즉시 정지 (낙상 등 상위 이벤트 발생 시 사용)
void stopObstacleMotors()
{
  leftObstacleActive  = false;
  rightObstacleActive = false;

  leftMotorPinState  = false;
  rightMotorPinState = false;

  digitalWrite(LEFT_VIBRATION_PIN, LOW);
  digitalWrite(RIGHT_VIBRATION_PIN, LOW);
}

// ======================================================
// 분절형 포고핀 Hot-plug 복구 보조 함수 - 최종 버전
//
// 핵심 원칙
//   1) 센서가 빠져도 ESP32 전체를 멈추지 않는다.
//   2) MPU6050은 0x68 고정 주소이므로 독립적으로 복구한다.
//   3) VL53L1X는 전원 재인가 시 0x29로 돌아오므로,
//      VL53L5CX의 LPn(GPIO25)을 먼저 LOW로 내려 0x29 충돌을 없앤 후 0x30으로 재할당한다.
//   4) VL53L5CX LPn은 I2C 통신을 가리는 용도로만 사용한다.
//      hot-plug 순간에는 이미 VL53L1X가 0x29로 들어와 충돌했을 수 있으므로
//      stopRanging()을 LPn LOW보다 먼저 호출하지 않는다.
//   5) VL53L5CX가 이후 프레임을 못 내보낼 때만 stop/start를 시도하고,
//      그래도 실패하면 begin() 전체 초기화를 fallback으로 사용한다.
//   6) I2C bus recovery는 "센서가 없다"는 이유만으로 실행하지 않는다.
//      SDA가 실제 LOW에 고착된 경우에만 SCL 9펄스 + STOP 조건을 만들어 복구한다.
// ======================================================

bool i2cDevicePresent(uint8_t address7)
{
  Wire.beginTransmission(address7);
  return (Wire.endTransmission() == 0);
}

bool isI2CBusStuckLow()
{
  // I2C transaction이 없는 health-check/recovery 구간에서 호출한다.
  // SDA가 계속 LOW면 slave가 transaction 도중 멈춘 bus-stuck 가능성이 높다.
  return (digitalRead(I2C_SDA_PIN) == LOW);
}

bool recoverI2CBusIfStuck()
{
  if (!isI2CBusStuckLow())
  {
    return false; // 단순 센서 미연결에는 버스 리셋을 하지 않음
  }

  Serial.println("[I2C 복구] SDA LOW 고착 감지 -> SCL 9펄스 + STOP 복구 시도");

  Wire.end();

  // ESP32 전용 open-drain 출력. HIGH를 쓰면 라인을 release하고 LOW를 쓰면 당긴다.
  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SCL_PIN, HIGH);
  delayMicroseconds(10);

  for (int i = 0; i < 9 && digitalRead(I2C_SDA_PIN) == LOW; i++)
  {
    digitalWrite(I2C_SCL_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(5);
  }

  // STOP condition: SCL HIGH인 상태에서 SDA LOW -> HIGH
  pinMode(I2C_SDA_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SDA_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(I2C_SCL_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(I2C_SDA_PIN, HIGH);
  delayMicroseconds(10);

  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  bool released = (digitalRead(I2C_SDA_PIN) == HIGH);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(tofReady ? 400000 : 100000);
  delay(30);

  if (released)
  {
    Serial.println("[I2C 복구] SDA 해제 성공, Wire 재시작 완료");
  }
  else
  {
    Serial.println("[I2C 복구] SDA가 여전히 LOW -> 물리 접점/전원/센서 상태 확인 필요");
  }

  return released;
}

// VL53L1X 주소 레지스터(0x0001)에 실제 7비트 주소값을 직접 기록한다.
// ESP32만 재부팅되고 VL53L1X 전원은 유지되어 0x30에 남아 있을 때,
// 새 SparkFun 객체가 기본주소 0x29부터 다시 초기화할 수 있게 되돌리는 용도다.
bool writeVL53L1XRegister8(uint8_t deviceAddress7, uint16_t reg, uint8_t value)
{
  Wire.beginTransmission(deviceAddress7);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}


// VL53L1X Hot-plug 판별은 raw ID 레지스터를 직접 읽지 않는다.
// SparkFun SFEVL53L1X::begin() 자체가 내부에서 checkID()를 수행한 뒤
// VL53L1X_SensorInit()까지 실행한다.
// 이 코드에서는 VL53L5CX를 LPn LOW로 먼저 격리한 뒤 새 SFEVL53L1X 객체의
// begin() 성공 여부를 실제 VL53L1X 존재/초기화 판정으로 사용한다.

void resetStairDetectionState()
{
  lastStairDistanceCM = -1.0f;
  stairOverCount = 0;
  stairAlertActive = false;
  stairFilterIdx = 0;

  for (int i = 0; i < STAIR_FILTER_SAMPLES; i++)
  {
    stairFilterBuf[i] = -1.0f;
  }
}

void resetObstacleDetectionState()
{
  lastDistanceCM = -1.0f;
  obstacleSide = OBSTACLE_NONE;
  tofFilterIdxLeft = 0;
  tofFilterIdxRight = 0;

  for (int i = 0; i < TOF_FILTER_SAMPLES; i++)
  {
    tofFilterBufLeft[i] = -1.0f;
    tofFilterBufRight[i] = -1.0f;
  }

  stopObstacleMotors();
}

void markMPUDisconnected()
{
  if (!mpuReady) return;

  mpuReady = false;
  mpuMissCount = 0;
  fallState = IDLE;
  lastAccelMag = 0.0f;

  Serial.println("[HOTPLUG] MPU6050 분리 감지 -> 낙상 감지만 일시 중지");
}

bool initializeMPU6050HotPlug()
{
  if (!i2cDevicePresent(MPU6050_ADDR_7))
  {
    return false;
  }

  // MPU 초기화는 100kHz에서 하고, VL53L5CX가 살아 있으면 다시 400kHz로 복귀한다.
  Wire.setClock(100000);

  if (!mpu.begin())
  {
    if (tofReady) Wire.setClock(400000);
    return false;
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

  if (tofReady) Wire.setClock(400000);

  mpuReady = true;
  mpuMissCount = 0;
  fallState = IDLE;
  lastAccelMag = 0.0f;

  Serial.println("[HOTPLUG] MPU6050 초기화/복구 완료 (0x68)");
  return true;
}

void destroyStairSensorObject()
{
  if (stairSensor != nullptr)
  {
    delete stairSensor;
    stairSensor = nullptr;
  }
}

// 반드시 VL53L5CX의 LPn(GPIO25)이 LOW인 상태에서 호출한다.
// 이 상태에서는 0x29에 VL53L1X만 존재할 수 있으므로 안전하게 0x30으로 재할당할 수 있다.
// ESP32 I2C 컨트롤러만 깨끗하게 재시작한다.
// 센서 전원/주소는 직접 건드리지 않는다.
void restartI2CControllerForHotPlug()
{
  Wire.end();
  delay(20);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  delay(250);
}

void resetStairAddressObservation()
{
  stairStable29Count = 0;
  stairStable30Count = 0;
  stairLastAddressSampleTime = 0;
}

StairInitResult initializeVL53L1XWithL5Disabled()
{
  stairSensorReady = false;
  resetStairDetectionState();
  destroyStairSensorObject();

  Wire.setClock(100000);

  // 포고핀 재결합 직후에는 충분히 기다린다.
  delay(1000);

  bool ackAt29 = i2cDevicePresent(VL53L1X_DEFAULT_ADDR_7);
  bool ackAt30 = i2cDevicePresent(VL53L1X_NEW_ADDR_7);

  Serial.printf("[HOTPLUG] 주소 스캔 참고값: 0x29=%s, 0x30=%s\n",
                ackAt29 ? "ACK" : "NONE",
                ackAt30 ? "ACK" : "NONE");

  // ====================================================
  // v8 핵심:
  // 스캔에서 0x30이 안 잡혀도 먼저 0x30 직접 접속을 시도한다.
  //
  // 네 기존 정상 코드의 fallback:
  //   stairSensor.setI2CAddress(VL53L1X_NEW_ADDR);
  //   if (stairSensor.checkID()) {
  //       stairSensor.init();
  //   }
  //
  // SparkFun 객체 내부 통신 주소를 0x30으로 맞춘 뒤 checkID()를 직접 수행한다.
  // 따라서 단순 Wire ACK 스캔 결과에만 의존하지 않는다.
  // ====================================================
  Serial.println("[HOTPLUG] 1순위: 스캔 결과와 무관하게 VL53L1X 0x30 직접 접속 시도");

  stairSensor = new SFEVL53L1X();

  if (stairSensor == nullptr)
  {
    Serial.println("[HOTPLUG] VL53L1X 객체 메모리 할당 실패");
    return STAIR_INIT_FAILED_NO_CONFLICT;
  }

  // 기존 정상 코드와 동일한 방법으로 객체의 대상 주소를 0x30으로 맞춤
  stairSensor->setI2CAddress(VL53L1X_NEW_ADDR);
  delay(100);

  if (stairSensor->checkID())
  {
    Serial.println("[HOTPLUG] VL53L1X 0x30 직접 checkID() 성공");

    int initResult = (int)stairSensor->init();

    if (initResult == 0)
    {
      stairSensor->setDistanceModeShort();
      stairSensor->setTimingBudgetInMs(50);
      stairSensor->setIntermeasurementPeriod(HCSR04_SAMPLE_INTERVAL);
      stairSensor->startRanging();

      resetStairDetectionState();
      stairSensorReady = true;
      stairMissCount = 0;

      Serial.println("[HOTPLUG] VL53L1X 0x30 직접 복구 성공");
      return STAIR_INIT_OK;
    }

    Serial.printf("[HOTPLUG] VL53L1X 0x30 checkID 성공했지만 init() 실패: %d\n",
                  initResult);
  }
  else
  {
    Serial.println("[HOTPLUG] VL53L1X 0x30 직접 checkID() 실패");
  }

  destroyStairSensorObject();

  // ====================================================
  // 2순위: 0x30 직접 접속이 실패했을 때만 0x29 경로 시도
  //
  // VL53L5CX는 LPn LOW 상태이므로 정상이라면 0x29에는
  // 재부팅된 VL53L1X만 남아 있어야 한다.
  // ====================================================
  delay(250);

  ackAt29 = i2cDevicePresent(VL53L1X_DEFAULT_ADDR_7);
  ackAt30 = i2cDevicePresent(VL53L1X_NEW_ADDR_7);

  Serial.printf("[HOTPLUG] 0x30 직접 접속 실패 후 재확인: 0x29=%s, 0x30=%s\n",
                ackAt29 ? "ACK" : "NONE",
                ackAt30 ? "ACK" : "NONE");

  if (ackAt29)
  {
    stairSensor = new SFEVL53L1X();

    if (stairSensor == nullptr)
    {
      Serial.println("[HOTPLUG] VL53L1X 객체 메모리 할당 실패");
      return STAIR_INIT_VERIFIED_AT_29_BUT_FAILED;
    }

    Serial.println("[HOTPLUG] 2순위: VL53L1X 0x29에서 begin() 1회 시도");

    int beginResult = (int)stairSensor->begin();

    if (beginResult == 0)
    {
      Serial.println("[HOTPLUG] VL53L1X 0x29 begin() 성공 -> 0x30으로 변경");

      stairSensor->setI2CAddress(VL53L1X_NEW_ADDR);
      delay(150);

      // 주소 변경 후에는 checkID()를 우선 사용한다.
      // ACK 스캔은 참고용으로만 출력한다.
      bool idAt30 = stairSensor->checkID();
      bool scan29 = i2cDevicePresent(VL53L1X_DEFAULT_ADDR_7);
      bool scan30 = i2cDevicePresent(VL53L1X_NEW_ADDR_7);

      Serial.printf("[HOTPLUG] 0x29 -> 0x30 변경 후: checkID=%s, scan29=%s, scan30=%s\n",
                    idAt30 ? "OK" : "FAIL",
                    scan29 ? "ACK" : "NONE",
                    scan30 ? "ACK" : "NONE");

      if (idAt30)
      {
        stairSensor->setDistanceModeShort();
        stairSensor->setTimingBudgetInMs(50);
        stairSensor->setIntermeasurementPeriod(HCSR04_SAMPLE_INTERVAL);
        stairSensor->startRanging();

        resetStairDetectionState();
        stairSensorReady = true;
        stairMissCount = 0;

        Serial.println("[HOTPLUG] VL53L1X 0x29 -> 0x30 변경 및 복구 성공");
        return STAIR_INIT_OK;
      }

      Serial.println("[HOTPLUG] begin()은 성공했지만 0x30 checkID() 실패");
    }
    else
    {
      Serial.printf("[HOTPLUG] VL53L1X 0x29 begin() 실패: %d\n", beginResult);
    }

    destroyStairSensorObject();

    // 0x29가 남아 있는 동안 L5CX를 다시 올리면 주소 충돌 가능성이 있으므로 격리 유지.
    return STAIR_INIT_VERIFIED_AT_29_BUT_FAILED;
  }

  // 0x29가 없고 0x30 직접 접속도 실패했다면
  // 아직 센서 미연결 또는 접점/전원 불안정 상태로 본다.
  if (ackAt30)
  {
    Serial.println("[HOTPLUG] 스캔상 0x30 ACK는 있으나 직접 checkID 실패 -> 다음 주기에 다시 0x30부터 시도");
    return STAIR_INIT_FAILED_NO_CONFLICT;
  }

  Serial.println("[HOTPLUG] VL53L1X 미검출 -> 다음 주기에 다시 0x30 직접 접속부터 시도");
  return STAIR_INIT_ABSENT;
}

bool initializeVL53L5CXNormal()
{
  Serial.println("VL53L5CX 초기화 중... (최대 10초 소요)");

  digitalWrite(VL53L5CX_LPN_PIN, HIGH);
  tofCommsIsolated = false;
  delay(200);

  disableCore0WDT();
  disableCore1WDT();

  bool tofOk = tof.begin();

  enableCore0WDT();
  enableCore1WDT();

  if (!tofOk)
  {
    tofReady = false;
    resetObstacleDetectionState();
    Serial.println("VL53L5CX를 찾을 수 없습니다. 장애물 감지만 비활성 상태로 계속 실행합니다.");
    return false;
  }

  tof.setResolution(8 * 8);
  tofZones = tof.getResolution();
  tof.setRangingFrequency(15);
  Wire.setClock(400000);

  if (!tof.startRanging())
  {
    tofReady = false;
    resetObstacleDetectionState();
    Serial.println("VL53L5CX startRanging() 실패 -> 장애물 감지 OFF");
    return false;
  }

  resetObstacleDetectionState();
  tofReady = true;
  tofMissCount = 0;
  lastTofFrameTime = millis(); // 첫 프레임이 올 시간을 확보하기 위한 grace 기준

  Serial.printf("VL53L5CX 준비 완료. (존 수: %d)\n", tofZones);
  Serial.printf("ToF 상/하 필터 적용 행 범위: row %d ~ %d (필요 시 코드 상단 TOF_ROW_MIN/MAX 값 조정)\n",
                TOF_ROW_MIN, TOF_ROW_MAX);
  return true;
}

void markVL53L5CXUnavailable(const char* reason)
{
  if (tofReady)
  {
    Serial.print("[HOTPLUG] VL53L5CX 장애물 감지 일시 중지: ");
    Serial.println(reason);
  }

  tofReady = false;
  tofMissCount = 0;
  resetObstacleDetectionState();
}

// 주소 충돌이 없는 상태에서만 호출한다.
// 1차: 기존 객체에서 stop/start만 재시도
// 2차: 그래도 실패하면 begin() 전체 초기화
bool recoverVL53L5CXAfterCommsFailure()
{
  if (tofCommsIsolated)
  {
    return false;
  }

  digitalWrite(VL53L5CX_LPN_PIN, HIGH);
  delay(30);
  Wire.setClock(400000);

  // 기존 객체와 펌웨어가 살아 있으면 가장 가벼운 stop/start부터 시도한다.
  if (tof.isConnected())
  {
    tof.stopRanging();
    delay(10);

    if (tof.startRanging())
    {
      tofReady = true;
      tofMissCount = 0;
      lastTofFrameTime = millis();
      Serial.println("[HOTPLUG] VL53L5CX stop/start 복구 성공");
      return true;
    }
  }

  Serial.println("[HOTPLUG] VL53L5CX stop/start 실패 -> begin() 전체 재초기화 시도");
  return initializeVL53L5CXNormal();
}

// 실행 중 VL53L1X가 빠졌다 다시 연결되었을 때 복구.
// 중요: 이 시점에는 재연결된 VL53L1X가 이미 0x29일 수 있으므로
//       0x29로 I2C 명령(stopRanging 포함)을 보내기 전에 VL53L5CX LPn을 먼저 LOW로 만든다.
bool recoverVL53L1XHotPlug()
{
  // VL53L1X가 빠진 동안 v5는 VL53L5CX를 이미 LPn LOW로 유지한다.
  // 혹시 다른 경로에서 호출되어도 가장 먼저 LOW로 강제한다.
  digitalWrite(VL53L5CX_LPN_PIN, LOW);
  tofCommsIsolated = true;
  resetObstacleDetectionState();

  Serial.println("[HOTPLUG] VL53L1X 복구 시도 (VL53L5CX LPn LOW 격리 유지)");

  delay(100);

  Serial.printf("[HOTPLUG-DIAG] GPIO25(LPn)=%d, 0x29=%s, 0x30=%s\n",
                digitalRead(VL53L5CX_LPN_PIN),
                i2cDevicePresent(VL53L1X_DEFAULT_ADDR_7) ? "ACK" : "NONE",
                i2cDevicePresent(VL53L1X_NEW_ADDR_7) ? "ACK" : "NONE");

  StairInitResult stairResult = initializeVL53L1XWithL5Disabled();

  // VL53L1X가 아직 정상 0x30으로 복구되지 않았다면
  // 절대 VL53L5CX를 HIGH로 올리지 않는다.
  // 이유: 0x29에 VL53L1X가 남아 있는 순간 L5CX를 올리면 즉시 주소 충돌이 재발한다.
  if (stairResult != STAIR_INIT_OK)
  {
    digitalWrite(VL53L5CX_LPN_PIN, LOW);
    tofCommsIsolated = true;
    resetObstacleDetectionState();

    if (stairResult == STAIR_INIT_ABSENT)
    {
      Serial.println("[HOTPLUG] 계단 센서 미연결 -> 재결합 대기, VL53L5CX I2C는 충돌 방지를 위해 격리 유지");
    }
    else if (stairResult == STAIR_INIT_VERIFIED_AT_29_BUT_FAILED)
    {
      Serial.println("[HOTPLUG] VL53L1X가 0x29에 남아 있음 -> VL53L5CX 절대 올리지 않고 다음 주기에 재시도");
    }
    else
    {
      Serial.println("[HOTPLUG] VL53L1X 복구 미완료 -> VL53L5CX 격리 유지 후 다음 주기에 재시도");
    }

    if (isI2CBusStuckLow())
    {
      recoverI2CBusIfStuck();
    }

    return false;
  }

  // 여기까지 왔다는 것은 VL53L1X가 정상적으로 0x30에서 살아난 상태.
  // 이제서야 VL53L5CX를 0x29에 복귀시켜도 주소 충돌이 없다.
  Serial.println("[HOTPLUG] VL53L1X가 0x30에서 정상 복구됨 -> VL53L5CX I2C 재개");

  digitalWrite(VL53L5CX_LPN_PIN, HIGH);
  tofCommsIsolated = false;
  delay(150);
  Wire.setClock(400000);

  // 기존에 L5CX가 정상 초기화되어 있었다면 LPn은 comms만 막았으므로
  // 대부분 별도 begin() 없이 다시 통신된다.
  if (tofReady)
  {
    bool connected = false;

    for (int i = 0; i < 3; i++)
    {
      if (tof.isConnected())
      {
        connected = true;
        break;
      }
      delay(50);
    }

    if (connected)
    {
      tofMissCount = 0;
      lastTofFrameTime = millis();
      Serial.println("[HOTPLUG] VL53L5CX I2C 통신 재개 성공 (기존 ranging 유지)");
    }
    else
    {
      Serial.println("[HOTPLUG] VL53L5CX 재개 후 응답 없음 -> 이제 주소 충돌이 없으므로 안전하게 전체 복구");
      markVL53L5CXUnavailable("LPn HIGH 후 응답 없음");
      recoverVL53L5CXAfterCommsFailure();
    }
  }
  else
  {
    // 부팅 당시 계단 센서가 없어서 L5CX를 아예 초기화하지 않았던 경우
    initializeVL53L5CXNormal();
  }

  Serial.println("[HOTPLUG] 계단 센서 재결합 복구 성공");
  return true;
}

void markStairSensorDisconnected()
{
  if (!stairSensorReady) return;

  stairSensorReady = false;
  stairMissCount = 0;
  resetStairDetectionState();
  destroyStairSensorObject();

  // 재결합 순간 0x29 충돌 방지를 위해 VL53L5CX를 미리 격리한다.
  digitalWrite(VL53L5CX_LPN_PIN, LOW);
  tofCommsIsolated = true;
  resetObstacleDetectionState();

  // v7: 바로 복구하지 않는다.
  // 포고핀/전원 접점이 완전히 안정될 시간을 먼저 준다.
  stairDisconnectedAtTime = millis();
  stairRecoveryCooldownUntil = 0;
  resetStairAddressObservation();

  Serial.println("[HOTPLUG] VL53L1X 분리 감지 -> 계단 감지 OFF");
  Serial.println("[HOTPLUG] VL53L5CX LPn LOW 격리");
  Serial.println("[HOTPLUG] 재결합 후 최소 3초 안정화 + 주소 5회 연속 확인 후 복구합니다.");
}

// loop()에서 계속 호출. 실제 I2C health check는 500ms마다만 수행한다.
void checkSensorHotPlugRecovery()
{
  unsigned long now = millis();

  if (now - lastSensorHealthCheckTime < SENSOR_HEALTH_CHECK_INTERVAL_MS)
  {
    return;
  }
  lastSensorHealthCheckTime = now;

  // ---------- MPU6050 ----------
  bool mpuPresentNow = i2cDevicePresent(MPU6050_ADDR_7);

  if (mpuReady)
  {
    if (!mpuPresentNow)
    {
      mpuMissCount++;
      if (mpuMissCount >= SENSOR_MISS_LIMIT)
      {
        markMPUDisconnected();
        if (isI2CBusStuckLow()) recoverI2CBusIfStuck();
      }
    }
    else
    {
      mpuMissCount = 0;
    }
  }
  else if (mpuPresentNow)
  {
    Serial.println("[HOTPLUG] MPU6050 재연결 감지 -> 재초기화 시도");
    if (initializeMPU6050HotPlug())
    {
      // MPU가 먼저 살아나더라도 VL53L1X는 별도의 안정화/주소 관찰 시간을 그대로 지킨다.
    }
    else if (isI2CBusStuckLow())
    {
      recoverI2CBusIfStuck();
    }
  }

  // ---------- VL53L1X (정상 주소 0x30일 때만 직접 health check 가능) ----------
  if (stairSensorReady)
  {
    bool stairPresentNow = i2cDevicePresent(VL53L1X_NEW_ADDR_7);

    if (!stairPresentNow)
    {
      stairMissCount++;
      if (stairMissCount >= SENSOR_MISS_LIMIT)
      {
        markStairSensorDisconnected();
        lastStairRecoveryAttemptTime = 0;
        if (isI2CBusStuckLow()) recoverI2CBusIfStuck();
      }
    }
    else
    {
      stairMissCount = 0;
    }
  }

  // ---------- VL53L5CX ----------
  // VL53L1X가 0x30에 정상적으로 있을 때는 0x29가 확실히 VL53L5CX이므로 주소 ping도 사용할 수 있다.
  if (tofReady && !tofCommsIsolated && stairSensorReady)
  {
    bool tofPresentNow = i2cDevicePresent(VL53L1X_DEFAULT_ADDR_7); // 0x29
    if (!tofPresentNow)
    {
      tofMissCount++;
      if (tofMissCount >= SENSOR_MISS_LIMIT)
      {
        markVL53L5CXUnavailable("0x29 응답 없음");
        if (isI2CBusStuckLow()) recoverI2CBusIfStuck();
      }
    }
    else
    {
      tofMissCount = 0;
    }
  }

  // 주소 ping만으로는 hot-plug된 VL53L1X(0x29)와 VL53L5CX(0x29)를 구분할 수 없으므로
  // 실제 ranging frame이 일정 시간 안 들어오는지도 별도로 본다.
  if (tofReady && !tofCommsIsolated && lastTofFrameTime > 0 &&
      now - lastTofFrameTime > TOF_NO_FRAME_TIMEOUT_MS)
  {
    markVL53L5CXUnavailable("2초 이상 ranging frame 없음");
    if (isI2CBusStuckLow()) recoverI2CBusIfStuck();
  }

  // ---------- VL53L1X 재연결 탐색 ----------
  // v7는 빠른 반복 초기화를 하지 않는다.
  // 1) 분리/부팅 실패 후 최소 3초 대기
  // 2) 400ms 간격으로 주소 관찰
  // 3) 같은 주소가 5회 연속 안정적으로 보일 때만 초기화
  // 4) 실패하면 5초 cooldown 후 다시 주소 안정성 관찰부터 시작
  if (!stairSensorReady)
  {
    // 주소 충돌 방지를 위해 관찰/복구가 끝날 때까지 L5CX는 계속 격리
    if (!tofCommsIsolated)
    {
      digitalWrite(VL53L5CX_LPN_PIN, LOW);
      tofCommsIsolated = true;
      resetObstacleDetectionState();
    }

    if (stairDisconnectedAtTime == 0)
    {
      stairDisconnectedAtTime = now;
      resetStairAddressObservation();
    }

    // 이전 초기화 실패 후 cooldown 중
    if (stairRecoveryCooldownUntil != 0 &&
        (long)(now - stairRecoveryCooldownUntil) < 0)
    {
      // 아직 기다리는 중
    }
    else if (now - stairDisconnectedAtTime >= STAIR_RECONNECT_SETTLE_MS &&
             (stairLastAddressSampleTime == 0 ||
              now - stairLastAddressSampleTime >= STAIR_ADDR_SAMPLE_INTERVAL_MS))
    {
      stairLastAddressSampleTime = now;

      bool at29 = i2cDevicePresent(VL53L1X_DEFAULT_ADDR_7);
      bool at30 = i2cDevicePresent(VL53L1X_NEW_ADDR_7);

      // 0x30 우선.
      // 다만 두 주소가 동시에 ACK면 상태가 애매하므로 카운트를 모두 초기화하고 더 기다린다.
      if (at30 && !at29)
      {
        stairStable30Count++;
        stairStable29Count = 0;

        Serial.printf("[HOTPLUG-WAIT] 0x30 안정성 확인 %u/%u\n",
                      stairStable30Count, STAIR_ADDR_STABLE_REQUIRED);
      }
      else if (at29 && !at30)
      {
        stairStable29Count++;
        stairStable30Count = 0;

        Serial.printf("[HOTPLUG-WAIT] 0x29 안정성 확인 %u/%u\n",
                      stairStable29Count, STAIR_ADDR_STABLE_REQUIRED);
      }
      else if (at29 && at30)
      {
        stairStable29Count = 0;
        stairStable30Count = 0;
        Serial.println("[HOTPLUG-WAIT] 0x29/0x30 동시 ACK -> 아직 불안정, 초기화하지 않고 더 기다림");
      }
      else
      {
        // 둘 다 없으면 아직 센서 미연결 또는 접점 안정화 중
        if (stairStable29Count != 0 || stairStable30Count != 0)
        {
          Serial.println("[HOTPLUG-WAIT] 주소 응답 사라짐 -> 안정성 카운트 초기화");
        }
        stairStable29Count = 0;
        stairStable30Count = 0;
      }

      bool stable30 = stairStable30Count >= STAIR_ADDR_STABLE_REQUIRED;
      bool stable29 = stairStable29Count >= STAIR_ADDR_STABLE_REQUIRED;

      if (stable30 || stable29)
      {
        Serial.printf("[HOTPLUG] 주소 안정 확인 완료 -> %s 우선 복구 시작\n",
                      stable30 ? "0x30" : "0x29");

        lastStairRecoveryAttemptTime = now;
        resetStairAddressObservation();

        bool recovered = recoverVL53L1XHotPlug();

        if (recovered)
        {
          stairDisconnectedAtTime = 0;
          stairRecoveryCooldownUntil = 0;
        }
        else
        {
          // 실패했다고 바로 begin()을 연속 호출하지 않는다.
          stairRecoveryCooldownUntil = millis() + STAIR_RECOVERY_RETRY_INTERVAL_MS;
          stairDisconnectedAtTime = millis();
          resetStairAddressObservation();

          Serial.printf("[HOTPLUG] 복구 실패 -> %lu ms 쉬고 주소 안정성부터 다시 관찰\n",
                        STAIR_RECOVERY_RETRY_INTERVAL_MS);
        }
      }
    }
  }

  // ---------- VL53L5CX 독립 복구 ----------
  // VL53L1X가 정상 0x30에 있으면 주소 충돌이 없으므로 stop/start -> begin fallback을 안전하게 수행 가능.
  if (!tofReady && !tofCommsIsolated && stairSensorReady &&
      (lastTofRecoveryAttemptTime == 0 ||
       now - lastTofRecoveryAttemptTime >= TOF_RECOVERY_RETRY_INTERVAL_MS))
  {
    lastTofRecoveryAttemptTime = now;
    recoverVL53L5CXAfterCommsFailure();
  }
}

// ======================================================
// 낙상 확정 시 알림
// ======================================================

void triggerFallAlert()
{
  Serial.println(">>> 낙상 감지! (진동 없음, BLE 알림만 전송) <<<");

  sendMessage("FALL_DETECTED");

  obstacleSide = OBSTACLE_NONE;
  stopObstacleMotors();
  stairAlertActive = false;
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
// VL53L5CX: 좌/우 최소 거리 읽기
// ======================================================

bool readToFDistances(float &outLeftCM, float &outRightCM)
{
  // VL53L1X가 빠진 동안에는 0x29 충돌 방지를 위해 VL53L5CX의 LPn을 LOW로 유지한다.
  // 이때는 VL53L5CX I2C에 절대 접근하지 않는다.
  if (!tofReady || tofCommsIsolated)
  {
    return false;
  }

  if (!tof.isDataReady())
  {
    return false;
  }

  VL53L5CX_ResultsData measurementData;
  if (!tof.getRangingData(&measurementData))
  {
    return false;
  }

  // 실제 ranging frame을 정상 수신한 시각 기록 (VL53L5CX health 감시용)
  lastTofFrameTime = millis();
  tofMissCount = 0;

  // debugPrintTofRows(measurementData);

  float leftMinMM  = -1.0f;
  float rightMinMM = -1.0f;

  for (int i = 0; i < tofZones; i++)
  {
    if (measurementData.target_status[i] != 5) continue;

    int row = 7 - (i / 8);
    if (row < TOF_ROW_MIN || row > TOF_ROW_MAX) continue;

    float d = measurementData.distance_mm[i];
    if (d <= 0) continue;

    int col = i % 8;

    if (col < 4)
    {
      if (leftMinMM < 0 || d < leftMinMM) leftMinMM = d;
    }
    else
    {
      if (rightMinMM < 0 || d < rightMinMM) rightMinMM = d;
    }
  }

  outLeftCM  = (leftMinMM  < 0) ? -1.0f : leftMinMM  / 10.0f;
  outRightCM = (rightMinMM < 0) ? -1.0f : rightMinMM / 10.0f;

  return true;
}

// ======================================================
// 이동평균 필터 - 좌/우 공용
// ======================================================

float applyMovingAverage(float* buf, int& idx, float newSampleCM)
{
  if (newSampleCM < 0)
  {
    return -1.0f;
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
// 전방 장애물 감지 처리
// ======================================================

void checkObstacle()
{
  float rawLeft = -1.0f;
  float rawRight = -1.0f;

  bool newFrame = readToFDistances(rawLeft, rawRight);
  if (!newFrame)
  {
    return;
  }

  float left  = applyMovingAverage(tofFilterBufLeft,  tofFilterIdxLeft,  rawLeft);
  float right = applyMovingAverage(tofFilterBufRight, tofFilterIdxRight, rawRight);

  if (left >= 0 && right >= 0)      lastDistanceCM = min(left, right);
  else if (left >= 0)               lastDistanceCM = left;
  else if (right >= 0)              lastDistanceCM = right;

  bool leftObstacle  = (left  >= 0 && left  < OBSTACLE_THRESHOLD_CM);
  bool rightObstacle = (right >= 0 && right < OBSTACLE_THRESHOLD_CM);

  // 좌/우 전용 진동모터 상태 갱신 (실제 on/off 토글은 updateObstacleMotors()에서 매 loop 처리)
  leftObstacleActive  = leftObstacle;
  rightObstacleActive = rightObstacle;
  leftObstacleDistanceCM  = left;
  rightObstacleDistanceCM = right;

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
        Serial.println("장애물 없음 -> 좌/우 진동모터 OFF");
        break;
      case OBSTACLE_LEFT:
        Serial.printf("좌측 장애물 감지: %.1f cm -> 좌측 모터 진동 시작\n", left);
        break;
      case OBSTACLE_RIGHT:
        Serial.printf("우측 장애물 감지: %.1f cm -> 우측 모터 진동 시작\n", right);
        break;
      case OBSTACLE_BOTH:
        Serial.printf("양측 장애물 감지 (좌 %.1f / 우 %.1f cm) -> 좌우 모터 동시 진동\n", left, right);
        break;
    }
  }
}

// ======================================================
// VL53L1X: 하향 거리 측정
// ======================================================

float readVL53L1XDistanceCM()
{
  if (!stairSensorReady || stairSensor == nullptr)
  {
    return -1.0f;
  }

  if (!stairSensor->checkForDataReady())
  {
    return -1.0f;
  }

  int distMm = stairSensor->getDistance();
  stairSensor->clearInterrupt();

  return distMm / 10.0f;
}

// ======================================================
// 이동평균 필터 - 계단(VL53L1X)용
// ======================================================

float filteredStairDistance(float newSampleCM)
{
  if (newSampleCM < 0)
  {
    return -1.0f;
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
// ======================================================

void checkStairs()
{
  unsigned long now = millis();

  if (now - lastHCSR04SampleTime < HCSR04_SAMPLE_INTERVAL)
  {
    return;
  }
  lastHCSR04SampleTime = now;

  float raw = readVL53L1XDistanceCM();
  float dist = filteredStairDistance(raw);

  if (dist < 0)
  {
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
// ======================================================

void printStairDistance()
{
  unsigned long now = millis();

  if (now - lastStairPrintTime < STAIR_PRINT_INTERVAL)
  {
    return;
  }
  lastStairPrintTime = now;

  if (!stairSensorReady)
  {
    Serial.println("[계단 ToF] 센서 미연결/복구 대기 중");
  }
  else if (lastStairDistanceCM >= 0)
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
// 푸시버튼(GPIO33, 모멘터리) 체크
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

      if (buttonStableState == LOW)
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
//   - "GREEN"    : 초록불 감지 -> 진동 패턴 1회
//   - "YOLO_ON"  : YOLO 세션 시작 -> GPIO27 LED ON
//   - "YOLO_OFF" : YOLO 세션 종료 -> GPIO27 LED OFF
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
          digitalWrite(VIBRATION_PIN, HIGH);

          Serial.println("초록불 신호 감지 -> 짧게-길게-짧게-길게 진동 1회 재생");
        }
        else if (piRxBuffer == "YOLO_ON")
        {
          digitalWrite(YOLO_LED_PIN, HIGH);
          Serial.println("YOLO 세션 시작 신호 수신 -> LED(GPIO27) ON");
        }
        else if (piRxBuffer == "YOLO_OFF")
        {
          digitalWrite(YOLO_LED_PIN, LOW);
          Serial.println("YOLO 세션 종료 신호 수신 -> LED(GPIO27) OFF");
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
      greenPatternActive = false;
      digitalWrite(VIBRATION_PIN, LOW);
      return;
    }

    digitalWrite(VIBRATION_PIN, (greenPatternIndex % 2 == 0) ? HIGH : LOW);
  }
}

// ======================================================
// 진동 출력 일괄 처리
//   - 좌/우 장애물 모터(GPIO14/26): 항상 독립적으로 갱신 (메인 모터와 별개 하드웨어)
//   - 메인 모터(GPIO32): 초록불(1회) > 계단(연속 ON) 우선순위 유지
//   - 낙상 감지 시에는 triggerFallAlert()에서 stopObstacleMotors()로 장애물 모터도 즉시 정지
// ======================================================

void updateVibrationOutput()
{
  updateObstacleMotors();

  if (greenPatternActive)
  {
    playGreenPattern();
    return;
  }

  if (stairAlertActive)
  {
    digitalWrite(VIBRATION_PIN, HIGH);
    return;
  }

  digitalWrite(VIBRATION_PIN, LOW);
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

  // ---- 장애물(좌/우) 전용 진동모터 ----
  pinMode(LEFT_VIBRATION_PIN, OUTPUT);
  digitalWrite(LEFT_VIBRATION_PIN, LOW);

  pinMode(RIGHT_VIBRATION_PIN, OUTPUT);
  digitalWrite(RIGHT_VIBRATION_PIN, LOW);

  // ---- YOLO 세션 표시 LED (GPIO27) ----
  pinMode(YOLO_LED_PIN, OUTPUT);
  digitalWrite(YOLO_LED_PIN, LOW);

  // ---- 푸시버튼 (Pi5 YOLO 5분 세션 트리거) ----
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  lastButtonReading = digitalRead(BUTTON_PIN);
  buttonStableState = lastButtonReading;

  // ---- Pi5 UART2 초기화 (RX=16, TX=17, 115200) ----
  Serial2.begin(PI_UART_BAUD, SERIAL_8N1, PI_UART_RX_PIN, PI_UART_TX_PIN);
  Serial.println("Pi5 UART2 초기화 완료 (RX=16, TX=17, 115200)");

  // ---- I2C 공용 버스 초기화 (MPU6050 + VL53L5CX + VL53L1X) ----
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  delay(100);

  // ---- VL53L5CX를 리셋(비활성) 상태로 유지 ----
  pinMode(VL53L5CX_LPN_PIN, OUTPUT);
  digitalWrite(VL53L5CX_LPN_PIN, LOW);
  delay(100);

  // ---- MPU6050 초기화 (없어도 ESP32 전체는 계속 실행) ----
  if (!initializeMPU6050HotPlug())
  {
    Serial.println("MPU6050 미연결 -> 낙상 감지 OFF, 재연결을 자동 감시합니다.");
  }

  // ---- VL53L1X 초기화 (VL53L5CX는 현재 LPn LOW 상태) ----
  Serial.println("VL53L1X(계단) 초기화 중...");
  StairInitResult bootStairResult = initializeVL53L1XWithL5Disabled();

  if (bootStairResult == STAIR_INIT_OK)
  {
    // VL53L1X가 0x30에 정상적으로 자리잡은 뒤에만 VL53L5CX(0x29)를 올린다.
    initializeVL53L5CXNormal();
  }
  else
  {
    stairSensorReady = false;
    tofReady = false;

    // 부팅 시에도 VL53L1X가 없다면 L5CX를 LOW로 유지한다.
    // 그래야 나중에 VL53L1X를 꽂는 순간 기본주소 0x29 충돌이 생기지 않는다.
    digitalWrite(VL53L5CX_LPN_PIN, LOW);
    tofCommsIsolated = true;
    resetObstacleDetectionState();

    if (bootStairResult == STAIR_INIT_ABSENT)
    {
      Serial.println("VL53L1X 미연결 -> 계단 감지 OFF, 재연결 자동 감시");
    }
    else
    {
      Serial.println("VL53L1X 초기화 미완료 -> 재복구 자동 시도");
    }

    Serial.println("VL53L5CX는 0x29 충돌 방지를 위해 LPn LOW 대기 (VL53L1X 복구 후 자동 시작)");
    stairDisconnectedAtTime = millis();
    stairRecoveryCooldownUntil = 0;
    resetStairAddressObservation();

  }

  // ---- BLE 서버 초기화 ----
  initializeBluetooth();
}

// ======================================================
// 반복 실행 부분
// ======================================================

void loop()
{
  unsigned long currentTime = millis();

  // 분절부가 빠진 동안에는 해당 센서 함수를 호출하지 않는다.
  if (mpuReady && currentTime - lastAccelSampleTime >= ACCEL_SAMPLE_INTERVAL)
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

  checkSensorHotPlugRecovery();

  if (tofReady)
  {
    checkObstacle();
  }

  if (stairSensorReady)
  {
    checkStairs();
  }
  checkButton();
  checkPiUart();
  printStairDistance();
  updateVibrationOutput();

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