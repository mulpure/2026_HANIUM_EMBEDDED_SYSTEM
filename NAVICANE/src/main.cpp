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

  [이전 수정 내용 - VL53L1X 소프트웨어 리셋 도입 관련 부분은 원본과 동일, 생략 없이 유지]
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
#define LED_BUILTIN 27
#endif

const int LED_PIN = LED_BUILTIN;

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
#define VL53L1X_NEW_ADDR  0x60    // SparkFun 라이브러리 값 0x60 = 실제 7비트 I2C 주소 0x30
// ※ XSHUT은 여전히 3.3V 하드와이어 그대로 사용 (추가 GPIO 불필요)

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

SFEVL53L1X stairSensor;

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
  if (!tof.isDataReady())
  {
    return false;
  }

  VL53L5CX_ResultsData measurementData;
  if (!tof.getRangingData(&measurementData))
  {
    return false;
  }

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
  if (!stairSensor.checkForDataReady())
  {
    return -1.0f;
  }

  int distMm = stairSensor.getDistance();
  stairSensor.clearInterrupt();

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
  Wire.begin(21, 22);
  Wire.setClock(100000);
  delay(100);

  // ---- VL53L5CX를 리셋(비활성) 상태로 유지 ----
  pinMode(VL53L5CX_LPN_PIN, OUTPUT);
  digitalWrite(VL53L5CX_LPN_PIN, LOW);
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

  // ---- VL53L1X 초기화 (계단용) ----
  Serial.println("VL53L1X(계단) 초기화 중...");

  int vl53l1xInitResult = stairSensor.begin();
  Serial.printf("VL53L1X begin() 1차 시도(주소 0x29) 반환값: %d\n", vl53l1xInitResult);

  if (vl53l1xInitResult != 0)
  {
    Serial.println("0x29에서 실패 -> 이전 세션에서 0x30으로 재할당된 상태로 추정, 0x30으로 재시도...");

    stairSensor.setI2CAddress(VL53L1X_NEW_ADDR);

    if (!stairSensor.checkID())
    {
      Serial.println("VL53L1X를 찾을 수 없습니다. (0x29, 0x30 모두 실패)");
      Serial.println("배선 확인: SDA=21, SCL=22, VCC=3.3V, GND / 그래도 안 되면 전원을 완전히 껐다 켜보세요.");
      while (1) delay(100);
    }

    stairSensor.init();
    Serial.println("0x30에서 VL53L1X 통신 성공.");
  }
  else
  {
    stairSensor.setI2CAddress(VL53L1X_NEW_ADDR);
    delay(50);
  }

  stairSensor.setDistanceModeShort();
  stairSensor.setTimingBudgetInMs(50);
  stairSensor.setIntermeasurementPeriod(HCSR04_SAMPLE_INTERVAL);
  stairSensor.startRanging();

  Serial.println("VL53L1X 준비 완료. (주소=0x30)");

  // ---- VL53L5CX 초기화 ----
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

  tof.setResolution(8 * 8);
  tofZones = tof.getResolution();
  tof.setRangingFrequency(15);
  Wire.setClock(400000);
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

  checkObstacle();
  checkStairs();
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