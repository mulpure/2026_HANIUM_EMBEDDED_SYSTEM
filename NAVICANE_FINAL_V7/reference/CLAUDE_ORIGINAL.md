# 동행 지팡이(NAVCANE) ESP32 펌웨어 - 검토용 통합본

실제 프로젝트는 PlatformIO 폴더(src/ 여러 파일 + lib/)이고, 이 문서는 검토용으로 한 파일에 모은 것이다.
VL53L4CX 라이브러리는 공식 STM32duino VL53L4CX 1.1.0에서 I2C 함수 2개만 수정했으므로 diff만 포함한다.

## 파일 목록
- platformio.ini (21줄)
- src/config.h (81줄)
- src/logic.h (194줄)
- src/sensors.h (15줄)
- src/sensors.cpp (313줄)
- src/ble_link.h (9줄)
- src/ble_link.cpp (172줄)
- src/profiles.h (12줄)
- src/profiles.cpp (384줄)
- src/expansion.h (15줄)
- src/expansion.cpp (319줄)
- src/feedback.h (10줄)
- src/feedback.cpp (86줄)
- src/main.cpp (281줄)
- lib/STM32duino_VL53L4CX/src/vl53l4cx_class.cpp (수정 부분 diff)

---

## platformio.ini

```ini
; 동행 지팡이(NAVCANE) ESP32 펌웨어 - 최종본
[env:esp32dev]
platform = espressif32@^6.9.0          ; Arduino core 2.0.x (7.x/pioarduino 금지: ledcSetup API 다름)
board = esp32dev
framework = arduino
monitor_speed = 115200
monitor_filters = esp32_exception_decoder

; 앱 1.9MB + LittleFS 128KB (RFID 프로필 최대 500명 = 약 14KB)
; 파티션 변경 후 첫 부팅 때 LittleFS 마운트가 실패하면 펌웨어가 자동 포맷한다
board_build.partitions = min_spiffs.csv
board_build.filesystem = littlefs

build_flags =
  -DMFRC522_SPICLOCK=1000000u          ; 포고핀 경유 SPI 안정성 (기본 4MHz -> 1MHz)

; VL53L4CX는 lib/STM32duino_VL53L4CX (I2C 무한 재시도 패치본)를 자동 사용하므로 여기 넣지 않는다
; MPU6050은 레지스터 직접 접근이라 Adafruit 라이브러리 불필요
lib_deps =
  sparkfun/SparkFun VL53L5CX Arduino Library@^1.0.3
  miguelbalboa/MFRC522@^1.4.11
```

---

## src/config.h

```cpp
#pragma once
#include <stdint.h>

// =====================================================================
// 동행 지팡이(NAVCANE) 설정값. 실측 튜닝은 이 파일만 고치면 된다.
// =====================================================================
namespace cfg {

// ---------- BLE (앱 BleManager.kt와 동일해야 함) ----------
constexpr char DEVICE_NAME[] = "NAVCANE_ESP32";
constexpr char SERVICE_UUID[] = "12345678-1234-1234-1234-1234567890ab";
constexpr char CHARACTERISTIC_UUID[] = "abcd1234-1234-1234-1234-abcdef123456";
constexpr uint16_t BLE_LOCAL_MTU = 185;           // 앱 REQUEST_MTU = 185

// ---------- 핀 (확정 핀맵) ----------
constexpr uint8_t OBSTACLE_SDA = 18, OBSTACLE_SCL = 19, OBSTACLE_LPN = 25;   // VL53L5CX
constexpr uint8_t SEGMENT_SDA = 21, SEGMENT_SCL = 22;                       // 손잡이 포고핀 (MPU6050 + VL53L4CX)
constexpr uint8_t VIB_MAIN = 32, VIB_LEFT = 14, VIB_RIGHT = 26;
constexpr uint8_t VIB_CH_MAIN = 0, VIB_CH_LEFT = 2, VIB_CH_RIGHT = 4;      // LEDC 타이머 0/1/2
constexpr uint8_t SERVO = 23, SERVO_CHANNEL = 7;                           // LEDC 타이머 3
constexpr uint8_t BUTTON = 33;
constexpr uint8_t UART_RX = 16, UART_TX = 17;                              // Pi5
constexpr uint8_t SLOT_ID = 36, S1 = 4, S2 = 13, S3 = 35, S4 = 5;          // 확장 슬롯
// 미사용(예비): 39, 27, 15, 12, 34, 2

// ---------- 확장 슬롯 ID ----------
// ID 전압 = 3.3V x R / (10k + R): RFID 2.2k=0.60V / 리모컨 10k=1.65V / LED 33k=2.53V / 없음=3.3V
constexpr uint16_t RFID_ID_MIN_MV = 380, RFID_ID_MAX_MV = 850;
constexpr uint16_t REMOTE_ID_MIN_MV = 1250, REMOTE_ID_MAX_MV = 2050;
constexpr uint16_t LED_ID_MIN_MV = 2250, LED_ID_MAX_MV = 2850;
constexpr uint16_t EMPTY_ID_MIN_MV = 2950;
constexpr uint32_t ID_POLL_MS = 5;
constexpr uint32_t ID_SETTLE_MS = 300;       // 같은 ID가 이 시간 유지돼야 모듈 확정
constexpr uint8_t SLOT_LEAVE_SAMPLES = 3;    // 연속 3회(15ms) 다르면 즉시 출력 차단
constexpr uint32_t UNKNOWN_ID_WARN_MS = 300;

// ---------- 센서 공통 ----------
constexpr uint32_t I2C_TIMEOUT_MS = 25;
constexpr uint32_t SENSOR_HEALTH_MS = 500;
constexpr uint8_t SENSOR_MISS_LIMIT = 2;
constexpr uint32_t STAIR_STALE_MS = 600, OBSTACLE_STALE_MS = 600, ACCEL_STALE_MS = 120;

// 손잡이 포고핀 재결합: 200ms 간격으로 5회 연속 응답해야 초기화 (접점이 흔들리는 동안 초기화 방지)
constexpr uint32_t SEGMENT_PROBE_MS = 200;
constexpr uint8_t SEGMENT_STABLE_COUNT = 5;
constexpr uint32_t SEGMENT_INIT_COOLDOWN_MS = 2000;   // 초기화 실패 후 재시도 간격

// 장애물 VL53L5CX
constexpr uint32_t OBSTACLE_RETRY_MS = 3000;
constexpr uint8_t OBSTACLE_ROW_MIN = 0, OBSTACLE_ROW_MAX = 2;
constexpr bool SWAP_OBSTACLE_SIDES = false;   // 장착 후 좌우가 반대면 true
constexpr uint8_t OBSTACLE_FILTER_SAMPLES = 3;

// 계단 VL53L4CX
constexpr uint32_t STAIR_TIMING_BUDGET_US = 50000;
constexpr float DEFAULT_STAIR_CM = 170.0f;
// 키(height_code 14~19)별 계단 임계값. 키만으로 바닥까지의 비스듬한 거리가 정해지지 않으므로
// 조립된 지팡이로 실측하기 전까지는 전부 검증값 170cm로 둔다.
constexpr float STAIR_CM_BY_HEIGHT[6] = {170, 170, 170, 170, 170, 170};
constexpr bool STAIR_NO_TARGET_ALERT = true;  // 목표 없음(바닥이 멀다) = 계단

// ---------- 진동 / 서보 ----------
constexpr uint16_t VIB_WEAK_DUTY = 150, VIB_STRONG_DUTY = 255;   // 약 모드에서 모터가 안 돌면 WEAK 올리기
constexpr int SERVO_RELEASE_DEG = 0, SERVO_APPLY_DEG = 20;
constexpr uint16_t SERVO_MIN_US = 500, SERVO_MAX_US = 2400;

// ---------- 프로필 기본값 ----------
constexpr uint16_t DEFAULT_OBSTACLE_CM = 50;
constexpr uint8_t DEFAULT_HEIGHT_CODE = 17, DEFAULT_VIB_LEVEL = 1;
constexpr uint32_t REG_WAIT_MS = 15000;

// ---------- 리모컨 + 상부 초음파 ----------
constexpr uint32_t REMOTE_PULSE_MS = 300;
constexpr uint32_t ULTRASONIC_PERIOD_MS = 100;
constexpr uint32_t ULTRASONIC_TIMEOUT_US = 30000;    // 이보다 긴 에코 = 앞에 아무것도 없음(정상)
constexpr uint8_t ULTRASONIC_NO_ECHO_LIMIT = 10;     // 에코 펄스가 아예 없는 상태 10회 연속 = 모듈 무응답
constexpr float UPPER_OBSTACLE_CM = 120.0f;          // 상부 장애물 경고 시작
constexpr float UPPER_RELEASE_CM = 130.0f;           // 해제 (히스테리시스)
constexpr uint8_t UPPER_CONFIRM_COUNT = 2;

}  // namespace cfg
```

---

## src/logic.h

```cpp
#pragma once
// 하드웨어와 무관한 판단 로직 (PC에서 단위 테스트 가능)
#include <stdint.h>
#include <math.h>
#include "config.h"

namespace nav {

enum class Module : uint8_t { None, Rfid, Remote, Led, Unknown };

inline bool isModule(Module m) { return m == Module::Rfid || m == Module::Remote || m == Module::Led; }

inline Module identify(uint16_t mv) {
  if (mv >= cfg::RFID_ID_MIN_MV && mv <= cfg::RFID_ID_MAX_MV) return Module::Rfid;
  if (mv >= cfg::REMOTE_ID_MIN_MV && mv <= cfg::REMOTE_ID_MAX_MV) return Module::Remote;
  if (mv >= cfg::LED_ID_MIN_MV && mv <= cfg::LED_ID_MAX_MV) return Module::Led;
  return mv >= cfg::EMPTY_ID_MIN_MV ? Module::None : Module::Unknown;
}

inline bool elapsed(uint32_t now, uint32_t then, uint32_t period) {
  return uint32_t(now - then) >= period;
}

// 슬롯 게이트: ID가 ID_SETTLE_MS 동안 같아야 활성화, 연속 SLOT_LEAVE_SAMPLES회 다르면 비활성화
class SlotGate {
 public:
  Module active = Module::None;
  bool update(Module observed, uint32_t now) {
    const Module old = active;
    if (active != Module::None) {
      if (observed != active) {
        if (++leave >= cfg::SLOT_LEAVE_SAMPLES) {
          active = Module::None;
          leave = 0;
          candidate = observed;
          since = now;
        }
      } else {
        leave = 0;
      }
      return old != active;
    }
    if (observed != candidate) { candidate = observed; since = now; }
    if (isModule(candidate) && elapsed(now, since, cfg::ID_SETTLE_MS)) active = candidate;
    return old != active;
  }
 private:
  Module candidate = Module::None;
  uint32_t since = 0;
  uint8_t leave = 0;
};

// 계단 판정: 임계값 초과/목표 없음 2회 -> 경고, 임계값-5cm 미만 3회 -> 해제, 무효 측정 3회 -> 경고(안전측)
class StairDecision {
 public:
  bool alert = false;
  void reset() { alert = false; above = below = invalid = 0; }
  void sample(float cm, bool noTarget, float threshold) {
    const bool valid = isfinite(cm) && cm >= 0;
    const bool risk = (valid && cm > threshold) || (noTarget && cfg::STAIR_NO_TARGET_ALERT);
    if (risk) {
      below = invalid = 0;
      if (above < 2) ++above;
      if (above >= 2) alert = true;
    } else if (valid) {
      above = invalid = 0;
      if (cm < threshold - 5) {
        if (below < 3) ++below;
        if (below >= 3) alert = false;
      } else {
        below = 0;
      }
    } else {
      above = below = 0;
      if (invalid < 3) ++invalid;
      if (invalid >= 3) alert = true;
    }
  }
 private:
  uint8_t above = 0, below = 0, invalid = 0;
};

// MPU6050 +/-8g 원시값 -> 가속도 크기(m/s^2)
inline float accelMagnitude(const uint8_t *raw) {
  float sum = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    int16_t value = int16_t((uint16_t(raw[2 * i]) << 8) | raw[2 * i + 1]);
    float a = value * (9.80665f / 4096.0f);
    sum += a * a;
  }
  return sqrtf(sum);
}

// 낙상: 자유낙하(<5) -> 충격(>10) -> 0.5초 후 1초간 정지(7~12.5) (기존 펌웨어와 같은 기준)
class FallDetector {
 public:
  void reset() { state = 0; }
  bool sample(float m, uint32_t now) {
    if (!isfinite(m)) { reset(); return false; }
    switch (state) {
      case 0: if (m < 5.0f) { state = 1; since = now; } break;
      case 1:
        if (m > 10.0f) { state = 2; since = now; }
        else if (elapsed(now, since, 600)) reset();
        break;
      case 2:
        if (!elapsed(now, since, 500)) break;
        if (m < 7.0f || m > 12.5f) reset();
        else if (elapsed(now, since, 1500)) { reset(); return true; }
        break;
    }
    return false;
  }
 private:
  uint8_t state = 0;
  uint32_t since = 0;
};

// 장애물 3회 이동평균 (기존 펌웨어와 동일: 목표 없음은 즉시 -1)
class MovingAverage {
 public:
  void reset() { for (auto &v : buf) v = -1; idx = 0; }
  float add(float sample) {
    if (sample < 0) return -1;
    buf[idx] = sample;
    idx = (idx + 1) % cfg::OBSTACLE_FILTER_SAMPLES;
    float sum = 0; int n = 0;
    for (float v : buf) if (v >= 0) { sum += v; ++n; }
    return n ? sum / n : -1;
  }
 private:
  float buf[cfg::OBSTACLE_FILTER_SAMPLES] = {-1, -1, -1};
  uint8_t idx = 0;
};

// 상부 장애물: 120cm 미만 2회 -> 감지, 130cm 초과 또는 없음 2회 -> 해제
class UpperObstacle {
 public:
  bool active = false;
  float cm = -1;
  void reset() { active = false; cm = -1; over = clear = 0; }
  void sample(float distance) {
    if (distance >= 0 && distance < cfg::UPPER_OBSTACLE_CM) {
      clear = 0;
      if (over < cfg::UPPER_CONFIRM_COUNT) ++over;
      if (over >= cfg::UPPER_CONFIRM_COUNT) { active = true; cm = distance; }
    } else if (distance < 0 || distance > cfg::UPPER_RELEASE_CM) {
      over = 0;
      if (clear < cfg::UPPER_CONFIRM_COUNT) ++clear;
      if (clear >= cfg::UPPER_CONFIRM_COUNT) { active = false; cm = -1; }
    } else if (active) {
      cm = distance;   // 120~130cm 구간: 상태 유지
    }
  }
 private:
  uint8_t over = 0, clear = 0;
};

// 손잡이 센서 재결합 판단: 연속 SEGMENT_STABLE_COUNT회 응답해야 초기화, 실패하면 쿨다운
struct Rejoin {
  uint8_t stable = 0;
  uint32_t failAt = 0;
  bool failed = false;
  bool ready(bool presentNow, uint32_t now) {
    if (!presentNow) stable = 0;
    else if (stable < 255) ++stable;
    if (stable < cfg::SEGMENT_STABLE_COUNT) return false;
    return !failed || elapsed(now, failAt, cfg::SEGMENT_INIT_COOLDOWN_MS);
  }
  void initResult(bool ok, uint32_t now) {
    stable = 0;
    failed = !ok;
    if (!ok) failAt = now;
  }
};

// 초음파 한 번의 측정 결과 해석
enum class EchoResult : uint8_t { Distance, NoObstacle, NoResponse };
// sawRise: 에코 펄스 시작을 봤는지, done: 펄스 끝까지 봤는지, levelHigh: 타임아웃 시점에 ECHO가 아직 HIGH인지
inline EchoResult classifyEcho(bool done, uint32_t widthUs, bool sawRise, bool levelHigh, float &cm) {
  cm = -1;
  if (done) {
    if (widthUs >= 60 && widthUs <= cfg::ULTRASONIC_TIMEOUT_US) {
      cm = widthUs * 0.0343f / 2.0f;
      return EchoResult::Distance;
    }
    return EchoResult::NoObstacle;   // 너무 짧거나 너무 긴 펄스 = 모듈은 살아 있음
  }
  // 펄스가 시작됐거나 아직 HIGH = 반사가 없을 뿐 모듈은 정상(열린 공간)
  if (sawRise || levelHigh) return EchoResult::NoObstacle;
  return EchoResult::NoResponse;
}

}  // namespace nav
```

---

## src/sensors.h

```cpp
#pragma once
#include <Arduino.h>

// 센서 태스크(코어 0)가 채우고 메인 루프가 복사해 읽는 스냅샷
struct SensorSnapshot {
  bool mpuReady = false, stairReady = false, obstacleReady = false;
  float accel = 0, stairCm = -1, leftCm = -1, rightCm = -1;
  bool stairNoTarget = false;
  uint32_t accelAt = 0, stairAt = 0, obstacleAt = 0;
  uint32_t accelSeq = 0, stairSeq = 0, obstacleSeq = 0;
  uint32_t fallCount = 0, maxAccelGapMs = 0;
};

bool sensorsBegin();
SensorSnapshot sensorSnapshot();
```

---

## src/sensors.cpp

```cpp
// 센서 태스크 2개
//  - segmentTask : 손잡이 포고핀 버스(21/22) MPU6050 + 계단 VL53L4CX. 두 센서는 독립적으로 복구한다.
//  - obstacleTask: 장애물 버스(18/19) VL53L5CX. 느린 begin()(펌웨어 업로드)이 메인 루프를 막지 않는다.
#include "sensors.h"
#include "config.h"
#include "logic.h"
#include <Wire.h>
#include <new>
#include <SparkFun_VL53L5CX_Library.h>
#include <vl53l4cx_class.h>   // lib/STM32duino_VL53L4CX (I2C 무한 재시도 패치본)

namespace {
TwoWire segment(1);
VL53L4CX stair(&segment, -1);   // XSHUT 미사용 (Adafruit 보드 내부 풀업)
SparkFun_VL53L5CX obstacle;
SensorSnapshot shared;
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
nav::FallDetector fall;
nav::MovingAverage leftFilter, rightFilter;

void publishSegment(const SensorSnapshot &s) {
  portENTER_CRITICAL(&snapshotMux);
  shared.mpuReady = s.mpuReady; shared.stairReady = s.stairReady;
  shared.accel = s.accel; shared.accelAt = s.accelAt; shared.accelSeq = s.accelSeq;
  shared.stairCm = s.stairCm; shared.stairNoTarget = s.stairNoTarget;
  shared.stairAt = s.stairAt; shared.stairSeq = s.stairSeq;
  shared.fallCount = s.fallCount; shared.maxAccelGapMs = s.maxAccelGapMs;
  portEXIT_CRITICAL(&snapshotMux);
}

void publishObstacle(bool ready, float left, float right, uint32_t at, uint32_t seq) {
  portENTER_CRITICAL(&snapshotMux);
  shared.obstacleReady = ready; shared.leftCm = left; shared.rightCm = right;
  shared.obstacleAt = at; shared.obstacleSeq = seq;
  portEXIT_CRITICAL(&snapshotMux);
}

bool present(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

// ---------------- 손잡이 버스 ----------------

// VL53L4CX 0x010F~0x0110 = 0xEB 0xAA (VL53L0X/L1X 오장착 거부)
bool stairIdentity() {
  segment.beginTransmission(0x29);
  segment.write(uint8_t(0x01)); segment.write(uint8_t(0x0F));
  if (segment.endTransmission(false) != 0) return false;
  if (segment.requestFrom(uint8_t(0x29), uint8_t(2)) != 2) return false;
  const uint16_t id = (uint16_t(segment.read()) << 8) | segment.read();
  return id == 0xEBAA;
}

// SDA가 LOW로 고착되면 SCL 9펄스로 해제 (이 버스는 이 태스크만 사용)
bool recoverBusIfStuck() {
  if (digitalRead(cfg::SEGMENT_SDA) == HIGH) return false;
  segment.end();
  pinMode(cfg::SEGMENT_SDA, INPUT_PULLUP);
  pinMode(cfg::SEGMENT_SCL, OUTPUT_OPEN_DRAIN);
  digitalWrite(cfg::SEGMENT_SCL, HIGH);
  delayMicroseconds(10);
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
  segment.beginTransmission(0x68); segment.write(reg);
  if (segment.endTransmission(false) != 0) return false;
  if (segment.requestFrom(uint8_t(0x68), count) != count) return false;
  for (uint8_t i = 0; i < count; ++i) data[i] = segment.read();
  return true;
}

bool writeMpu(uint8_t reg, uint8_t value) {
  segment.beginTransmission(0x68); segment.write(reg); segment.write(value);
  return segment.endTransmission() == 0;
}

bool mpuIdentity() {
  uint8_t id = 0;
  return readMpu(0x75, &id, 1) && id == 0x68;
}

bool mpuConfigOk() {
  uint8_t range = 0, power = 0;
  return readMpu(0x1C, &range, 1) && (range & 0x18) == 0x10 &&
         readMpu(0x6B, &power, 1) && !(power & 0xC0);
}

bool initMpu() {
  if (!mpuIdentity()) return false;
  // PLL 클럭, DLPF 44Hz, 샘플 200Hz, 자이로 500dps, 가속도 +/-8g (기존 Adafruit 설정과 동일)
  if (!writeMpu(0x6B, 0x01) || !writeMpu(0x6C, 0x00) || !writeMpu(0x1A, 0x03) ||
      !writeMpu(0x19, 0x04) || !writeMpu(0x1B, 0x08) || !writeMpu(0x1C, 0x10)) return false;
  if (!mpuConfigOk()) return false;
  vTaskDelay(pdMS_TO_TICKS(50));
  fall.reset();
  Serial.println("[MPU] 준비 완료 (GPIO21/22, 0x68)");
  return true;
}

bool initStair() {
  if (!stairIdentity()) return false;
  // 재결합마다 드라이버 내부 상태를 새 객체로 초기화 (힙 할당 없음)
  stair.~VL53L4CX();
  new (&stair) VL53L4CX(&segment, -1);
  int status = stair.InitSensor(0x52);   // ST API는 8비트 주소(0x52 = 0x29 << 1)
  if (!status) status = stair.VL53L4CX_SetDistanceMode(VL53L4CX_DISTANCEMODE_LONG);
  if (!status) status = stair.VL53L4CX_SetMeasurementTimingBudgetMicroSeconds(cfg::STAIR_TIMING_BUDGET_US);
  if (!status) status = stair.VL53L4CX_StartMeasurement();
  Serial.printf("[STAIR] VL53L4CX 초기화 %s (status=%d)\n", status ? "실패" : "완료", status);
  return status == 0;
}

void segmentTask(void *) {
  segment.begin(cfg::SEGMENT_SDA, cfg::SEGMENT_SCL, 100000);
  segment.setTimeOut(cfg::I2C_TIMEOUT_MS);
  SensorSnapshot s;
  nav::Rejoin mpuRejoin, stairRejoin;
  uint32_t healthAt = 0, probeAt = 0, accelReadAt = 0, stairPollAt = 0, stairWatch = 0;
  uint8_t mpuMiss = 0, mpuHealthMiss = 0, stairMiss = 0, stairHealthMiss = 0;
  uint32_t lastFall = millis() - 10000;

  for (;;) {
    uint32_t now = millis();

    // 준비된 센서 상태 확인
    if (nav::elapsed(now, healthAt, cfg::SENSOR_HEALTH_MS)) {
      healthAt = now;
      if (recoverBusIfStuck()) {
        Serial.println("[SEGMENT] SDA LOW 고착 -> 버스 복구");
        s.mpuReady = s.stairReady = false; s.stairCm = -1; fall.reset();
      }
      if (s.mpuReady) {
        if (mpuConfigOk()) mpuHealthMiss = 0;
        else if (++mpuHealthMiss >= cfg::SENSOR_MISS_LIMIT) {
          s.mpuReady = false; fall.reset(); mpuHealthMiss = 0;
          Serial.println("[MPU] 응답 없음 -> 재결합 대기");
        }
      }
      if (s.stairReady) {
        if (stairIdentity()) stairHealthMiss = 0;
        else if (++stairHealthMiss >= cfg::SENSOR_MISS_LIMIT) {
          s.stairReady = false; s.stairCm = -1; stairHealthMiss = 0;
          Serial.println("[STAIR] 응답 없음 -> 재결합 대기");
        }
      }
    }

    // 미연결 센서 재결합 (MPU와 계단 ToF는 서로 독립)
    if (nav::elapsed(now, probeAt, cfg::SEGMENT_PROBE_MS)) {
      probeAt = now;
      if (!s.mpuReady && mpuRejoin.ready(mpuIdentity(), now)) {
        s.mpuReady = initMpu();
        mpuRejoin.initResult(s.mpuReady, millis());
        mpuMiss = mpuHealthMiss = 0;
      }
      if (!s.stairReady && stairRejoin.ready(stairIdentity(), now)) {
        s.stairReady = initStair();
        stairRejoin.initResult(s.stairReady, millis());
        if (s.stairReady) {
          s.stairAt = 0; s.stairCm = -1; s.stairNoTarget = false;
          stairWatch = millis(); stairMiss = stairHealthMiss = 0;
        }
      }
    }

    // 가속도 (15ms 주기)
    now = millis();
    if (s.mpuReady && nav::elapsed(now, accelReadAt, 15)) {
      accelReadAt = now;
      uint8_t raw[6] = {};
      if (readMpu(0x3B, raw, sizeof(raw))) {
        const uint32_t gap = now - s.accelAt;
        if (s.accelSeq && gap > s.maxAccelGapMs) s.maxAccelGapMs = gap;
        if (s.accelSeq && gap > cfg::ACCEL_STALE_MS) fall.reset();   // 공백 뒤에는 판정 재시작
        s.accel = nav::accelMagnitude(raw);
        s.accelAt = now; ++s.accelSeq; mpuMiss = 0;
        if (fall.sample(s.accel, now) && nav::elapsed(now, lastFall, 10000)) {
          lastFall = now; ++s.fallCount;
        }
      } else if (++mpuMiss >= cfg::SENSOR_MISS_LIMIT) {
        s.mpuReady = false; fall.reset(); mpuMiss = 0;
        Serial.println("[MPU] 읽기 실패 -> 재결합 대기");
      }
    }

    // 계단 거리 (측정 완료 시에만 읽음)
    if (s.stairReady && nav::elapsed(now, stairPollAt, 15)) {
      stairPollAt = now;
      uint8_t ready = 0;
      int status = stair.VL53L4CX_GetMeasurementDataReady(&ready);
      if (!status && ready) {
        VL53L4CX_MultiRangingData_t data = {};
        status = stair.VL53L4CX_GetMultiRangingData(&data);
        if (!status) {
          float nearest = -1;
          bool farHint = data.NumberOfObjectsFound == 0;
          for (uint8_t i = 0; i < data.NumberOfObjectsFound && i < VL53L4CX_MAX_RANGE_RESULTS; ++i) {
            const auto &r = data.RangeData[i];
            if (r.RangeStatus == VL53L4CX_RANGESTATUS_RANGE_VALID && r.RangeMilliMeter > 0) {
              float cm = r.RangeMilliMeter / 10.0f;
              if (nearest < 0 || cm < nearest) nearest = cm;
            } else if (r.RangeStatus == VL53L4CX_RANGESTATUS_SIGNAL_FAIL ||
                       r.RangeStatus == VL53L4CX_RANGESTATUS_OUTOFBOUNDS_FAIL) {
              farHint = true;   // 신호 약함/범위 밖 = 바닥이 멀다
            }
          }
          s.stairCm = nearest;
          s.stairNoTarget = nearest < 0 && farHint;
          s.stairAt = millis(); stairWatch = s.stairAt; ++s.stairSeq; stairMiss = 0;
          status = stair.VL53L4CX_ClearInterruptAndStartMeasurement();
        }
      }
      if (status && ++stairMiss >= cfg::SENSOR_MISS_LIMIT) {
        s.stairReady = false; s.stairCm = -1; stairMiss = 0;
        Serial.println("[STAIR] 측정 실패 -> 재결합 대기");
      }
      if (s.stairReady && nav::elapsed(millis(), stairWatch, cfg::STAIR_STALE_MS)) {
        s.stairReady = false; s.stairCm = -1;
        Serial.println("[STAIR] 측정 정지 -> 재결합 대기");
      }
    }

    publishSegment(s);
    vTaskDelay(pdMS_TO_TICKS(3));
  }
}

// ---------------- 장애물 버스 ----------------

bool initObstacle() {
  // LPn LOW 200ms -> 버스 재시작 -> LPn HIGH 500ms -> ACK 확인 후에만 begin() (기존 펌웨어 순서)
  digitalWrite(cfg::OBSTACLE_LPN, LOW);
  vTaskDelay(pdMS_TO_TICKS(200));
  Wire.end();
  Wire.begin(cfg::OBSTACLE_SDA, cfg::OBSTACLE_SCL, 100000);
  Wire.setTimeOut(500);
  digitalWrite(cfg::OBSTACLE_LPN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(500));
  if (!present(Wire, 0x29)) return false;
  if (!obstacle.begin(0x29, Wire)) return false;
  Wire.setClock(400000);
  return obstacle.setResolution(64) && obstacle.setRangingFrequency(15) && obstacle.startRanging();
}

void obstacleTask(void *) {
  Wire.begin(cfg::OBSTACLE_SDA, cfg::OBSTACLE_SCL, 100000);
  bool ready = false;
  uint32_t retry = millis() - cfg::OBSTACLE_RETRY_MS, at = 0, seq = 0;
  for (;;) {
    uint32_t now = millis();
    if (!ready && nav::elapsed(now, retry, cfg::OBSTACLE_RETRY_MS)) {
      leftFilter.reset(); rightFilter.reset();
      ready = initObstacle();
      retry = millis(); at = millis();
      Serial.printf("[OBSTACLE] VL53L5CX %s\n", ready ? "준비 완료" : "응답 없음 -> 재시도 대기");
      publishObstacle(false, -1, -1, at, seq);
    }
    if (ready && obstacle.isDataReady()) {
      VL53L5CX_ResultsData data;
      if (obstacle.getRangingData(&data)) {
        float left = -1, right = -1;
        for (int i = 0; i < 64; ++i) {
          const int row = 7 - i / 8;
          if (row < cfg::OBSTACLE_ROW_MIN || row > cfg::OBSTACLE_ROW_MAX ||
              data.target_status[i] != 5 || data.distance_mm[i] <= 0) continue;
          float cm = data.distance_mm[i] / 10.0f;
          float &side = (i % 8 < 4) ? left : right;
          if (side < 0 || cm < side) side = cm;
        }
        if (cfg::SWAP_OBSTACLE_SIDES) { float t = left; left = right; right = t; }
        left = leftFilter.add(left);
        right = rightFilter.add(right);
        at = millis(); ++seq;
        publishObstacle(true, left, right, at, seq);
      }
    }
    if (ready && nav::elapsed(millis(), at, 2500)) {   // 기존 펌웨어: 2.5초 프레임 없음 -> 재초기화
      ready = false; retry = millis() - cfg::OBSTACLE_RETRY_MS;
      Serial.println("[OBSTACLE] 2.5초 프레임 없음 -> 재초기화");
      publishObstacle(false, -1, -1, at, seq);
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
}  // namespace

SensorSnapshot sensorSnapshot() {
  portENTER_CRITICAL(&snapshotMux);
  SensorSnapshot result = shared;
  portEXIT_CRITICAL(&snapshotMux);
  return result;
}

bool sensorsBegin() {
  pinMode(cfg::OBSTACLE_LPN, OUTPUT);
  digitalWrite(cfg::OBSTACLE_LPN, LOW);
  // 코어 0 (BLE 스택과 같은 코어, 메인 루프는 코어 1)
  const auto a = xTaskCreatePinnedToCore(segmentTask, "segment", 12288, nullptr, 2, nullptr, 0);
  const auto b = xTaskCreatePinnedToCore(obstacleTask, "obstacle", 8192, nullptr, 1, nullptr, 0);
  return a == pdPASS && b == pdPASS;
}
```

---

## src/ble_link.h

```cpp
#pragma once
#include <Arduino.h>

void bleBegin();
void bleTick();                          // 메인 루프에서 호출: 송신 큐 처리, 광고 재시작
bool blePopCommand(String &command);     // 수신된 명령 한 줄
void sendMessage(const String &message); // 앱으로 이벤트 전송 (어느 태스크에서든 호출 가능)
bool bleConnected();
bool bleTakeSync();                      // 앱이 알림(CCCD)을 켠 직후 1회 true
```

---

## src/ble_link.cpp

```cpp
// BLE: 앱 BleManager.kt 기준
//  - 앱은 연결 -> MTU 185 -> 서비스 탐색 -> 알림(CCCD) 켜기 -> 이후 쓰기 순서로 동작
//  - 앱은 notify 1개를 메시지 1개로 처리(trim)한다 -> 메시지를 절대 나눠 보내지 않는다
//  - 앱의 RfidFragment는 "ACK,", "ERR," 로 시작하는 줄을 CFG 응답으로 해석한다
//    -> 펌웨어 내부 오류는 반드시 "ERROR:" 접두어를 쓴다
#include "ble_link.h"
#include "config.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <atomic>

namespace {
constexpr size_t MAX_LINE = 96;
struct Line { char text[MAX_LINE + 1]; };
QueueHandle_t rxQueue = nullptr, txQueue = nullptr;
BLEServer *server = nullptr;
BLECharacteristic *characteristic = nullptr;
BLE2902 *cccd = nullptr;
std::atomic<bool> connected{false}, syncPending{false}, disconnectedEvent{false}, fallPending{false};
std::atomic<uint16_t> mtu{23};
std::atomic<uint32_t> rxEpoch{0};
uint32_t advertiseAt = 0;
std::atomic<uint32_t> fallAt{0};
constexpr uint32_t FALL_PENDING_MAX_MS = 60000;   // 연결이 늦어져도 1분 지난 낙상은 보내지 않음(앱이 현재 위치를 붙이므로)

void queueCommand(const String &s) {
  if (!s.length() || !rxQueue) return;
  Line line = {};
  s.toCharArray(line.text, sizeof(line.text));
  if (xQueueSend(rxQueue, &line, 0) != pdTRUE) sendMessage("ERROR:QUEUE_FULL");
}

// 개행 없이 한 번에 와도 처리하는 명령 (기존 앱/디버그 호환)
bool wholeCommand(const String &s) {
  return s == "PING" || s == "REG" || s == "REG_CANCEL" || s == "GET_STATUS" ||
         s == "GET_ACCEL" || s == "GET_DISTANCE" || s == "GET_STAIR_DISTANCE" ||
         s == "GET_ULTRASONIC" || s == "LED_TEST";
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    connected = true; mtu = 23; ++rxEpoch;
  }
  void onDisconnect(BLEServer *) override {
    connected = false; syncPending = false; disconnectedEvent = true; mtu = 23; ++rxEpoch;
    if (cccd) cccd->setNotifications(false);   // 다음 연결에 이전 알림 상태가 남지 않게
  }
  void onMtuChanged(BLEServer *, esp_ble_gatts_cb_param_t *param) override {
    mtu = param->mtu.mtu;
  }
};

class CccdCallbacks : public BLEDescriptorCallbacks {
  void onWrite(BLEDescriptor *) override {
    if (cccd && cccd->getNotifications()) syncPending = true;
  }
};

// 수신: '\n' 단위로 조립 (앱은 "CFG,...\n", "REG\n" 형식으로 보냄)
class RxCallbacks : public BLECharacteristicCallbacks {
  String buffer;
  bool discard = false;
  uint32_t lastRx = 0, epoch = 0;
  void onWrite(BLECharacteristic *c) override {
    if (epoch != rxEpoch.load()) { epoch = rxEpoch.load(); buffer = ""; discard = false; }
    if (uint32_t(millis() - lastRx) > 1000) { buffer = ""; discard = false; }
    lastRx = millis();
    const std::string chunk = c->getValue();
    for (char ch : chunk) {
      if (ch == '\r') continue;
      if (ch == '\n') {
        if (discard) {
          // 너무 긴 CFG는 앱이 이해하는 형식 오류로 응답
          if (buffer.startsWith("CFG")) sendMessage("ERR,CFG,FORMAT");
          else sendMessage("ERROR:COMMAND_TOO_LONG");
        } else {
          buffer.trim();
          queueCommand(buffer);
        }
        buffer = ""; discard = false;
      } else if (!discard) {
        if (ch == '\0') { discard = true; continue; }
        buffer += ch;
        if (buffer.length() > MAX_LINE) { discard = true; buffer = buffer.substring(0, 3); }
      }
    }
    String pending = buffer; pending.trim();
    if (!discard && wholeCommand(pending)) { queueCommand(pending); buffer = ""; }
  }
};
}  // namespace

void bleBegin() {
  rxQueue = xQueueCreate(12, sizeof(Line));
  txQueue = xQueueCreate(32, sizeof(Line));
  if (!rxQueue || !txQueue) { Serial.println("[BLE] 큐 생성 실패"); return; }
  BLEDevice::init(cfg::DEVICE_NAME);
  BLEDevice::setMTU(cfg::BLE_LOCAL_MTU);
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks);
  BLEService *service = server->createService(cfg::SERVICE_UUID);
  characteristic = service->createCharacteristic(cfg::CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_NOTIFY);
  characteristic->setCallbacks(new RxCallbacks);
  cccd = new BLE2902;
  cccd->setCallbacks(new CccdCallbacks);
  characteristic->addDescriptor(cccd);
  characteristic->setValue("ESP32_READY");
  service->start();
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(cfg::SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();
  Serial.println("[BLE] 광고 시작: " + String(cfg::DEVICE_NAME));
}

void sendMessage(const String &s) {
  Serial.println("[EVENT] " + s);
  // 낙상은 큐가 차 있어도 반드시 다음 전송으로 (연결이 끊겨 있으면 재연결 후 전송)
  if (s == "FALL_DETECTED") { fallAt = millis(); fallPending = true; return; }
  if (!connected || !txQueue || !cccd || !cccd->getNotifications()) return;
  if (s.length() > MAX_LINE - 1) { Serial.println("[BLE] 메시지가 너무 김: 전송 안 함"); return; }
  Line line = {};
  String framed = s;
  if (!framed.endsWith("\n")) framed += '\n';
  framed.toCharArray(line.text, sizeof(line.text));
  if (xQueueSend(txQueue, &line, 0) != pdTRUE) Serial.println("[BLE] 송신 큐 가득 참");
}

bool blePopCommand(String &s) {
  Line line;
  if (!rxQueue || xQueueReceive(rxQueue, &line, 0) != pdTRUE) return false;
  s = line.text;
  return true;
}

bool bleConnected() { return connected; }
bool bleTakeSync() { return syncPending.exchange(false); }

void bleTick() {
  static uint32_t lastSend = 0;
  if (disconnectedEvent.exchange(false)) {
    advertiseAt = millis();
    if (txQueue) xQueueReset(txQueue);
    if (rxQueue) xQueueReset(rxQueue);
    Serial.println("[BLE] 연결 해제");
  }
  if (!connected && advertiseAt && uint32_t(millis() - advertiseAt) >= 500) {
    server->startAdvertising();
    advertiseAt = 0;
    Serial.println("[BLE] 광고 재시작");
  }
  if (!connected || !cccd || !cccd->getNotifications()) return;
  if (uint32_t(millis() - lastSend) < 12) return;   // notify 간격 (안드로이드 수신 버퍼 보호)

  Line line;
  bool fall = fallPending.exchange(false) && uint32_t(millis() - fallAt.load()) <= FALL_PENDING_MAX_MS;
  if (fall) strcpy(line.text, "FALL_DETECTED\n");
  else if (xQueueReceive(txQueue, &line, 0) != pdTRUE) return;

  const size_t len = strlen(line.text);
  const size_t payload = mtu.load() > 3 ? mtu.load() - 3 : 20;
  if (len > payload) {
    // MTU 협상 전(20바이트)이면 메시지가 잘린다. 앱은 연결 직후 MTU 185를 요청하므로 정상 상황에선 발생 안 함
    Serial.printf("[BLE] MTU(%u)보다 긴 메시지 잘림: %s", (unsigned)mtu.load(), line.text);
  }
  characteristic->setValue(reinterpret_cast<uint8_t *>(line.text), min(len, payload));
  characteristic->notify();
  lastSend = millis();
}
```

---

## src/profiles.h

```cpp
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
```

---

## src/profiles.cpp

```cpp
// RFID 사용자 프로필 (앱 RfidFragment.kt / RfidConfig.kt 기준)
//  앱 -> CFG,<name>,<obstacle_cm>,<height_code>,<vib>\n   (대기 설정으로만 보관, 즉시 적용하지 않음)
//  앱 -> REG\n  -> 15초 동안 카드 대기 -> UID별 저장 후 그 프로필 적용
//  응답은 앱이 파싱하는 ACK,/ERR,/REG,/APPLY,/UNREG, 형식만 사용
#include "profiles.h"
#include "feedback.h"
#include "ble_link.h"
#include "expansion.h"
#include "config.h"
#include <LittleFS.h>
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
constexpr char BAD_PATH[] = "/profiles.bad";
bool mountProfiles() {
  if (LittleFS.begin(false)) return true;
  // 파일시스템 자체가 마운트되지 않으면(첫 업로드, 파티션 변경) 펌웨어로는 복구할 방법이 없으므로 포맷한다.
  Serial.println("[PROFILE] LittleFS 마운트 실패 -> 포맷");
  if (!LittleFS.format()) return false;
  return LittleFS.begin(false);
}
void loadProfiles() {
  profileCount = 0; profileStoreReady = mountProfiles();
  if (!profileStoreReady) { Serial.println("[PROFILE] 저장소 사용 불가 -> 등록 불가(REG,FSERR)"); return; }
  if (!LittleFS.exists(PROFILE_FILE_PATH)) return;
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
    // 손상 파일은 지우지 않고 /profiles.bad로 보관, 등록은 빈 상태에서 계속 가능
    profileCount = 0;
    LittleFS.remove(BAD_PATH);
    LittleFS.rename(PROFILE_FILE_PATH, BAD_PATH);
    Serial.println("[PROFILE] 프로필 파일 손상 -> /profiles.bad 로 보관, 빈 상태로 시작");
  } else {
    profileCount = count;
    Serial.printf("[PROFILE] %d명 로드 (구형식=%d)\n", count, !header);
  }
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
  if (result == 1) { sendMessage("ERR,CFG,FORMAT"); return; }
  if (result == 2) { sendMessage("ERR,CFG,RANGE"); return; }
  pendingCfg = cfg;   // 등록용 대기 설정. 현재 사용자 설정은 바꾸지 않는다
  Serial.printf("[RFID] 대기 설정 수신: %s\n", pendingCfg.name);
  sendMessage("ACK,CFG");
}

void startRegistration()
{
  if (!pendingCfg.valid) { sendMessage("REG,NOCFG"); return; }
  if (!profileStoreReady) { sendMessage("REG,FSERR"); return; }
  regWaiting = true;               // 이미 대기 중이면 15초 재시작
  regStartTime = millis();
  // 이미 대고 있는 카드도 다음 폴링에서 등록된다
  Serial.println("[RFID] 등록 대기 시작 (15초)");
  sendMessage("REG,WAIT," + String(REG_WAIT_MS / 1000));
}

void updateRegistrationTimeout()
{
  if (!regWaiting) return;
  if (millis() - regStartTime < REG_WAIT_MS) return;
  regWaiting = false;              // 대기 설정은 유지
  Serial.println("[RFID] 등록 대기 타임아웃");
  sendMessage("REG,TIMEOUT");
  feedbackStart(Feedback::Failed);
}

void registerCard(const uint8_t *uid, uint8_t len, const String &uidHex)
{
  regWaiting = false;
  int idx = findProfileIndex(uid, len);
  bool isNew = idx < 0;
  if (isNew && profileCount >= PROFILE_MAX_COUNT)
  {
    sendMessage("REG,FULL");
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
    sendMessage("REG,FSERR");
    feedbackStart(Feedback::Failed);
    return;
  }
  profiles[target] = rec;          // 저장 성공 후에만 RAM 반영
  if (isNew) profileCount++;

  applyProfileValues(rec.name, rec.obstacleCm, rec.heightCode, rec.vib);
  Serial.printf("[RFID] 등록 %s: %s (%s), 총 %d명\n",
                isNew ? "신규" : "덮어쓰기", uidHex.c_str(), rec.name, profileCount);
  sendMessage("REG,OK," + uidHex + (isNew ? ",NEW," : ",UPDATE,") + String(rec.name));
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
      sendMessage("APPLY," + uidHex + "," + String(r.name));
      feedbackStart(Feedback::Applied);
    }
    else
    {
      Serial.println("[RFID] 미등록 카드 " + uidHex + " -> 기본 모드");
      applyDefaultProfile();
      sendMessage("UNREG," + uidHex);
      feedbackStart(Feedback::Unregistered);
    }
  }

  memcpy(lastHandledUid, uid, len);
  lastHandledUidLen = len;
  lastHandledTime = now;
}


void profilesBegin() { applyDefaultProfile(); loadProfiles(); }
void profilesCard(const uint8_t *uid, uint8_t length) {
  if (length == 4 || length == 7 || length == 10) handleCardDetected(uid, length);
}
void profilesTick() { updateRegistrationTimeout(); }
bool profilesWaiting() { return regWaiting; }
void profilesCancelRegistration(const char *reason) {
  if (regWaiting) { regWaiting = false; sendMessage(String("REG,CANCEL,")+reason); }
}
bool profilesCommand(const String &c) {
  if (c == "CFG" || c.startsWith("CFG,")) { handleCfgCommand(c); return true; }
  if (c == "REG") {
    if (!expansionRfidReady()) sendMessage("REG,NO_RFID");
    else startRegistration();
    return true;
  }
  if (c == "REG_CANCEL") { profilesCancelRegistration("USER"); return true; }
  if (c.startsWith("SET_STAIR_CM,")) {
    long cm;
    // 보정모드용. 앱 RFID 화면이 ACK,/ERR,를 CFG 응답으로 해석하므로 다른 접두어를 쓴다
    if (!parseStrictUInt(c.substring(13), cm) || cm < 30 || cm > 600) sendMessage("ERROR:STAIR_RANGE");
    else { activeStairThresholdCM = cm; sendMessage("STAIR_CM:" + String(cm)); }
    return true;
  }
  return false;
}
```

---

## src/expansion.h

```cpp
#pragma once
#include <Arduino.h>
#include "logic.h"

void expansionBegin();
void expansionTick(uint8_t ledMask);   // ledMask: LED 모듈 Q0~Q4 (1 = 켜짐 = 정상)
void expansionRemotePress();
void expansionLedTest();
bool expansionRfidReady();
nav::Module expansionModule();
const char *expansionModuleName();
uint16_t expansionIdMv();
float expansionUltrasonicCm();
bool expansionUpperObstacle(float &cm);
bool expansionFault();
```

---

## src/expansion.cpp

```cpp
// 확장 슬롯 (7핀 포고핀 1개)
//  핀3 ID(GPIO36 ADC, 10k 풀업) 저항으로 모듈 판별, ID가 안정된 뒤에만 신호 출력
//  RFID(RC522): S1=SCK S2=MOSI S3=MISO S4=SS (RST는 모듈 안에서 3.3V)
//  리모컨+초음파: S1=TRIG S2=리모컨(PC817) S3=ECHO
//  LED(74HC595): S1=CLK S2=DATA S4=LATCH
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
bool spiRunning = false, faulted = false;

// ---------- RFID ----------
MFRC522 rfid(cfg::S4, MFRC522::UNUSED_PIN);
enum class RfidState { Off, Resetting, Ready, Fault };   // 리셋 2회 재시도 후 FAULT -> 재장착 대기
RfidState rfidState = RfidState::Off;
uint32_t resetAt = 0, healthAt = 0, cardAt = 0;
uint8_t retries = 0, healthMiss = 0, cardMiss = 0;
bool cardPresent = false;
uint8_t heldUid[10] = {}, heldUidLen = 0;

// ---------- 리모컨 + 초음파 ----------
uint32_t remoteOffAt = 0;
float ultrasonicCm = -1;
uint32_t ultrasonicAt = 0, triggerAt = 0, triggerUs = 0;
uint8_t noEchoCount = 0;
bool echoWaiting = false;
nav::UpperObstacle upper;
portMUX_TYPE echoMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool echoArmed = false, echoSawRise = false, echoDone = false;
volatile uint32_t echoRiseUs = 0, echoWidthUs = 0;

// ---------- LED ----------
uint32_t ledTestAt = 0, ledAt = 0;
bool ledTesting = false;
uint8_t lastLedMask = 0xFF;

void IRAM_ATTR echoIsr() {
  portENTER_CRITICAL_ISR(&echoMux);
  if (echoArmed) {
    if (digitalRead(cfg::S3)) {
      echoRiseUs = micros(); echoSawRise = true;
    } else if (echoSawRise) {
      echoWidthUs = uint32_t(micros() - echoRiseUs); echoDone = true; echoArmed = false;
    }
  }
  portEXIT_CRITICAL_ISR(&echoMux);
}

void setUpper(float cm) {
  const bool was = upper.active;
  upper.sample(cm);
  if (upper.active != was) sendMessage(upper.active ? "상부 장애물" : "상부 장애물 해제");
}

// 모든 슬롯 신호를 입력(고임피던스)으로
void highZ() {
  detachInterrupt(digitalPinToInterrupt(cfg::S3));
  portENTER_CRITICAL(&echoMux); echoArmed = echoDone = false; portEXIT_CRITICAL(&echoMux);
  if (spiRunning) { SPI.end(); spiRunning = false; }
  pinMode(cfg::S1, INPUT); pinMode(cfg::S2, INPUT); pinMode(cfg::S3, INPUT); pinMode(cfg::S4, INPUT);
  echoWaiting = false; remoteOffAt = 0; rfidState = RfidState::Off;
  if (upper.active) { upper.reset(); sendMessage("상부 장애물 해제"); }
  upper.reset(); ultrasonicCm = -1;
}

void fault(const char *message, bool latchRfid) {
  if (!faulted) { sendMessage(message); feedbackStart(Feedback::Fault); }
  faulted = true;
  if (latchRfid) {
    profilesCancelRegistration("RFID_FAULT");
    highZ();
    rfidState = RfidState::Fault;   // 모듈을 뺐다 다시 꽂을 때까지 출력 중지
  }
}

bool versionValid(uint8_t v) {
  return v == 0x91 || v == 0x92 || v == 0x88 || v == 0x90 || v == 0x12 || v == 0xB2;
}

void softReset() {
  rfid.PCD_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
  rfidState = RfidState::Resetting; resetAt = millis(); cardPresent = false;
}

// 라이브러리 PCD_Init()과 같은 설정. delay(50)이 없고 RF 타임아웃만 25ms -> 2.5ms
void configureRfid() {
  rfid.PCD_WriteRegister(MFRC522::TxModeReg, 0);
  rfid.PCD_WriteRegister(MFRC522::RxModeReg, 0);
  rfid.PCD_WriteRegister(MFRC522::ModWidthReg, 0x26);
  rfid.PCD_WriteRegister(MFRC522::TModeReg, 0x80);
  rfid.PCD_WriteRegister(MFRC522::TPrescalerReg, 0xA9);
  rfid.PCD_WriteRegister(MFRC522::TReloadRegH, 0);
  rfid.PCD_WriteRegister(MFRC522::TReloadRegL, 0x64);
  rfid.PCD_WriteRegister(MFRC522::TxASKReg, 0x40);
  rfid.PCD_WriteRegister(MFRC522::ModeReg, 0x3D);
  rfid.PCD_AntennaOn();
}

const char *nameOf(nav::Module m) {
  switch (m) {
    case nav::Module::Rfid: return "RFID";
    case nav::Module::Remote: return "리모컨";
    case nav::Module::Led: return "LED";
    case nav::Module::Unknown: return "인식 불가";
    default: return "없음";
  }
}

void activate(nav::Module type) {
  faulted = false; retries = healthMiss = cardMiss = 0;
  heldUidLen = 0; cardPresent = false; ultrasonicCm = -1; noEchoCount = 0; upper.reset();
  if (type == nav::Module::Rfid) {
    digitalWrite(cfg::S4, HIGH); pinMode(cfg::S4, OUTPUT);
    SPI.begin(cfg::S1, cfg::S3, cfg::S2, cfg::S4);   // 핀 지정 필수 (기본 SPI 핀 18/19/23은 I2C·서보)
    spiRunning = true;
    softReset();
    sendMessage("RFID 연결");
  } else if (type == nav::Module::Remote) {
    digitalWrite(cfg::S1, LOW); pinMode(cfg::S1, OUTPUT);
    digitalWrite(cfg::S2, LOW); pinMode(cfg::S2, OUTPUT);
    pinMode(cfg::S3, INPUT);
    attachInterrupt(digitalPinToInterrupt(cfg::S3), echoIsr, CHANGE);
    triggerAt = millis() - cfg::ULTRASONIC_PERIOD_MS;
    sendMessage("리모컨 연결");
  } else if (type == nav::Module::Led) {
    digitalWrite(cfg::S1, LOW); pinMode(cfg::S1, OUTPUT);
    digitalWrite(cfg::S2, LOW); pinMode(cfg::S2, OUTPUT);
    digitalWrite(cfg::S4, LOW); pinMode(cfg::S4, OUTPUT);
    lastLedMask = 0xFF; ledTesting = false; ledAt = millis() - 100;
    sendMessage("LED 연결");
  }
  Serial.printf("[SLOT] %s 연결 (ID=%u mV)\n", nameOf(type), idMv);
}

void rfidTick() {
  if (rfidState == RfidState::Fault || rfidState == RfidState::Off) return;
  const uint32_t now = millis();
  if (rfidState == RfidState::Resetting) {
    if (!nav::elapsed(now, resetAt, 60)) return;
    const uint8_t version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    bool ok = versionValid(version) && !(rfid.PCD_ReadRegister(MFRC522::CommandReg) & 0x10);
    if (ok) {
      configureRfid();
      ok = rfid.PCD_ReadRegister(MFRC522::TxASKReg) == 0x40 &&
           (rfid.PCD_ReadRegister(MFRC522::TxControlReg) & 3) == 3;
    }
    if (!ok) {
      if (retries++ < 2) softReset();
      else fault("RFID 응답 없음", true);
    } else {
      rfidState = RfidState::Ready; healthMiss = 0; retries = 0; healthAt = now;
      Serial.printf("[RFID] 준비 완료 (Version=0x%02X)\n", version);
    }
    return;
  }
  if (nav::elapsed(now, healthAt, 1000)) {
    healthAt = now;
    bool ok = versionValid(rfid.PCD_ReadRegister(MFRC522::VersionReg)) &&
              rfid.PCD_ReadRegister(MFRC522::TxASKReg) == 0x40;
    if (ok) healthMiss = 0;
    else if (++healthMiss >= 2) { softReset(); return; }
  }
  if (!nav::elapsed(now, cardAt, 150)) return;
  cardAt = now;
  byte atqa[2], size = sizeof(atqa);
  auto status = rfid.PICC_WakeupA(atqa, &size);   // IDLE/HALT 카드 모두 깨움
  if (status != MFRC522::STATUS_OK && status != MFRC522::STATUS_COLLISION) {
    if (cardMiss < 3) ++cardMiss;
    if (cardMiss >= 3) { cardPresent = false; heldUidLen = 0; }   // 카드 떨어짐
    return;
  }
  if (!rfid.PICC_ReadCardSerial()) return;
  const uint8_t len = rfid.uid.size;
  const bool valid = len == 4 || len == 7 || len == 10;
  const bool different = valid && (!cardPresent || len != heldUidLen ||
                                   memcmp(heldUid, rfid.uid.uidByte, len) != 0);
  rfid.PICC_HaltA();
  if (valid) {
    // 같은 카드를 계속 대고 있으면 다시 적용하지 않음 (등록 대기 중이면 바로 등록)
    if (different || profilesWaiting()) profilesCard(rfid.uid.uidByte, len);
    memcpy(heldUid, rfid.uid.uidByte, len); heldUidLen = len;
    cardPresent = true; cardMiss = 0;
  }
}

void ultrasonicTick() {
  const uint32_t now = millis();
  if (remoteOffAt && int32_t(now - remoteOffAt) >= 0) {
    digitalWrite(cfg::S2, LOW); remoteOffAt = 0;
  }
  if (echoWaiting) {
    bool done, sawRise; uint32_t width;
    portENTER_CRITICAL(&echoMux);
    done = echoDone; sawRise = echoSawRise; width = echoWidthUs;
    portEXIT_CRITICAL(&echoMux);
    const bool timedOut = uint32_t(micros() - triggerUs) >= cfg::ULTRASONIC_TIMEOUT_US + 5000;
    if (done || timedOut) {
      echoWaiting = false;
      portENTER_CRITICAL(&echoMux); echoArmed = false; echoDone = false; portEXIT_CRITICAL(&echoMux);
      float cm;
      const auto result = nav::classifyEcho(done, width, sawRise, digitalRead(cfg::S3) == HIGH, cm);
      if (result == nav::EchoResult::NoResponse) {
        if (noEchoCount < 255) ++noEchoCount;
        if (noEchoCount >= cfg::ULTRASONIC_NO_ECHO_LIMIT) {
          ultrasonicCm = -1;
          if (upper.active) { upper.reset(); sendMessage("상부 장애물 해제"); }
          fault("초음파 응답 없음", false);
        }
      } else {
        noEchoCount = 0;
        if (faulted) { faulted = false; sendMessage("초음파 복구"); }
        ultrasonicCm = cm; ultrasonicAt = now;
        setUpper(cm);   // cm = -1 이면 "앞에 없음"
      }
    }
  }
  if (!echoWaiting && nav::elapsed(now, triggerAt, cfg::ULTRASONIC_PERIOD_MS)) {
    triggerAt = now; triggerUs = micros(); echoWaiting = true;
    portENTER_CRITICAL(&echoMux); echoSawRise = echoDone = false; echoArmed = true; portEXIT_CRITICAL(&echoMux);
    digitalWrite(cfg::S1, HIGH); delayMicroseconds(10); digitalWrite(cfg::S1, LOW);
  }
}

void ledTick(uint8_t mask) {
  if (ledTesting) {
    uint32_t t = millis() - ledTestAt;
    if (t >= 3000) ledTesting = false;
    else mask = 1 << ((t / 500) % 5);   // 1번부터 차례로 켜기
  }
  if (mask == lastLedMask || !nav::elapsed(millis(), ledAt, 100)) return;
  ledAt = millis(); lastLedMask = mask;
  digitalWrite(cfg::S4, LOW);
  shiftOut(cfg::S2, cfg::S1, MSBFIRST, mask & 0x1F);
  digitalWrite(cfg::S4, HIGH);   // LATCH 상승 에지에서 출력 반영
}
}  // namespace

void expansionBegin() {
  highZ();
  pinMode(cfg::SLOT_ID, INPUT);
  analogSetPinAttenuation(cfg::SLOT_ID, ADC_11db);
}

void expansionTick(uint8_t mask) {
  if (nav::elapsed(millis(), lastIdPoll, cfg::ID_POLL_MS)) {
    lastIdPoll = millis();
    idMv = analogReadMilliVolts(cfg::SLOT_ID);
    observed = nav::identify(idMv);
    if (observed == nav::Module::Unknown) {
      if (!unknownAt) unknownAt = millis();
      if (!unknownWarned && nav::elapsed(millis(), unknownAt, cfg::UNKNOWN_ID_WARN_MS)) {
        unknownWarned = true;
        Serial.printf("[SLOT] 인식 불가 ID=%u mV -> ID 저항/접점 확인\n", idMv);
        sendMessage("확장모듈 인식 불가");
        feedbackStart(Feedback::Fault);
      }
    } else {
      unknownAt = 0; unknownWarned = false;
    }
    const auto previous = gate.active;
    if (gate.update(observed, millis())) {
      highZ();
      if (previous != nav::Module::None) {
        profilesCancelRegistration("DETACHED");
        sendMessage("확장모듈 분리");
        Serial.printf("[SLOT] %s 분리 (ID=%u mV)\n", nameOf(previous), idMv);
      }
      faulted = false;
      if (gate.active != nav::Module::None) activate(gate.active);
    }
  }
  if (gate.active == nav::Module::Rfid) rfidTick();
  else if (gate.active == nav::Module::Remote) ultrasonicTick();
  else if (gate.active == nav::Module::Led) ledTick(mask);
}

void expansionRemotePress() {
  if (gate.active != nav::Module::Remote) return;
  digitalWrite(cfg::S2, HIGH);
  remoteOffAt = millis() + cfg::REMOTE_PULSE_MS;
  sendMessage("리모컨 작동");
}

void expansionLedTest() {
  if (gate.active != nav::Module::Led) { sendMessage("ERROR:NO_LED_MODULE"); return; }
  ledTesting = true; ledTestAt = millis();
  sendMessage("LED 테스트");
}

bool expansionRfidReady() { return gate.active == nav::Module::Rfid && rfidState == RfidState::Ready; }

nav::Module expansionModule() {
  return gate.active == nav::Module::None && observed == nav::Module::Unknown ? nav::Module::Unknown : gate.active;
}

const char *expansionModuleName() { return nameOf(expansionModule()); }

uint16_t expansionIdMv() { return idMv; }

float expansionUltrasonicCm() {
  return gate.active == nav::Module::Remote && !nav::elapsed(millis(), ultrasonicAt, 600) ? ultrasonicCm : -1;
}

bool expansionUpperObstacle(float &cm) {
  cm = upper.cm;
  return gate.active == nav::Module::Remote && upper.active;
}

bool expansionFault() { return faulted || unknownWarned; }
```

---

## src/feedback.h

```cpp
#pragma once
#include <Arduino.h>

enum class Feedback : uint8_t { Green, Applied, Unregistered, Registered, Failed, Fault, Fall };

void feedbackBegin();
void feedbackStart(Feedback pattern);
// 좌/우 모터 = 하부 장애물, 중앙 모터 = 계단(연속) > 알림 패턴 > 상부 장애물(거리 비례 깜빡임)
void feedbackTick(bool stairAlert, float leftCm, float rightCm, bool upperActive, float upperCm);
void brakeSet(bool active);
```

---

## src/feedback.cpp

```cpp
#include "feedback.h"
#include "config.h"
#include "profiles.h"

namespace {
// 짝수 인덱스 = ON(ms), 홀수 = OFF(ms)
const uint16_t green[] = {150, 150, 500, 150, 150, 150, 500};     // 초록불 (기존)
const uint16_t applied[] = {120, 120, 120};                        // 등록 카드 적용: 짧게 2번
const uint16_t unregistered[] = {800};                             // 미등록 카드: 길게 1번
const uint16_t registered[] = {500, 200, 500};                     // 등록 성공: 길게 2번
const uint16_t failed[] = {100, 100, 100, 100, 100, 100, 100};     // 등록 실패/타임아웃: 짧게 4번
const uint16_t fault[] = {80, 120, 80, 120, 80};                   // 센서/모듈 고장: 짧게 3번
const uint16_t fall[] = {700, 200, 700, 200, 700};                 // 낙상: 길게 3번
const uint16_t *pattern = nullptr;
uint8_t count = 0, patternStep = 0, priority = 0;
uint32_t stepAt = 0;
bool servoState = false;

void motor(uint8_t channel, bool on) { ledcWrite(channel, on ? activeVibDuty : 0); }

void servoAngle(int degrees) {
  uint32_t us = map(constrain(degrees, 0, 180), 0, 180, cfg::SERVO_MIN_US, cfg::SERVO_MAX_US);
  ledcWrite(cfg::SERVO_CHANNEL, us * 65535UL / 20000UL);   // 50Hz, 16bit
}

// 거리 비례 깜빡임: 가까울수록 빠르게 (주기 60~500ms, 기존 펌웨어와 동일)
void pulseMotor(uint8_t channel, float cm, float threshold) {
  if (cm < 0 || cm >= threshold) { motor(channel, false); return; }
  uint32_t period = 60 + uint32_t(constrain(cm / threshold, 0.0f, 1.0f) * 440);
  motor(channel, millis() % period < period / 2);
}
}  // namespace

void feedbackBegin() {
  const uint8_t pins[] = {cfg::VIB_MAIN, cfg::VIB_LEFT, cfg::VIB_RIGHT};
  const uint8_t channels[] = {cfg::VIB_CH_MAIN, cfg::VIB_CH_LEFT, cfg::VIB_CH_RIGHT};
  for (int i = 0; i < 3; ++i) {
    ledcSetup(channels[i], 5000, 8);
    ledcAttachPin(pins[i], channels[i]);
    ledcWrite(channels[i], 0);
  }
  ledcSetup(cfg::SERVO_CHANNEL, 50, 16);
  ledcAttachPin(cfg::SERVO, cfg::SERVO_CHANNEL);
  servoAngle(cfg::SERVO_RELEASE_DEG);
  Serial.println("[SERVO] GPIO23 브레이크 서보 -> 해제 위치");
}

void feedbackStart(Feedback type) {
  const uint8_t p = type == Feedback::Fall ? 3 : type == Feedback::Fault ? 2 : 1;
  if (pattern && priority > p) return;   // 낙상 > 고장 > 일반 안내
  priority = p;
#define SELECT(a) pattern = a; count = sizeof(a) / sizeof(a[0]); break
  switch (type) {
    case Feedback::Green: SELECT(green);
    case Feedback::Applied: SELECT(applied);
    case Feedback::Unregistered: SELECT(unregistered);
    case Feedback::Registered: SELECT(registered);
    case Feedback::Failed: SELECT(failed);
    case Feedback::Fault: SELECT(fault);
    case Feedback::Fall: SELECT(fall);
  }
#undef SELECT
  patternStep = 0;
  stepAt = millis();
}

void feedbackTick(bool stairAlert, float left, float right, bool upperActive, float upperCm) {
  pulseMotor(cfg::VIB_CH_LEFT, left, activeObstacleThresholdCM);
  pulseMotor(cfg::VIB_CH_RIGHT, right, activeObstacleThresholdCM);

  if (pattern && uint32_t(millis() - stepAt) >= pattern[patternStep]) {
    stepAt = millis();
    if (++patternStep >= count) { pattern = nullptr; priority = 0; }
  }
  if (stairAlert) { motor(cfg::VIB_CH_MAIN, true); return; }          // 계단: 연속 진동
  if (pattern) { motor(cfg::VIB_CH_MAIN, patternStep % 2 == 0); return; }
  if (upperActive) pulseMotor(cfg::VIB_CH_MAIN, upperCm, cfg::UPPER_RELEASE_CM);
  else motor(cfg::VIB_CH_MAIN, false);
}

void brakeSet(bool active) {
  if (active == servoState) return;
  servoState = active;
  servoAngle(active ? cfg::SERVO_APPLY_DEG : cfg::SERVO_RELEASE_DEG);
  Serial.println(active ? "[SERVO] 계단 감지 -> 브레이크 작동" : "[SERVO] 계단 해제 -> 브레이크 해제");
}
```

---

## src/main.cpp

```cpp
/*
  동행 지팡이(NAVCANE) ESP32 펌웨어 - 최종본

  구조
    sensors.cpp   : 센서 태스크 2개 (코어 0)
                    - 손잡이 버스 GPIO21/22: MPU6050 낙상 + 계단 VL53L4CX (서로 독립 복구)
                    - 장애물 버스 GPIO18/19: VL53L5CX (느린 초기화가 메인 루프를 막지 않음)
    expansion.cpp : 확장 슬롯 1개 (RFID / 리모컨+상부 초음파 / LED 5개)
    profiles.cpp  : RFID 사용자 프로필 (CFG/REG, LittleFS, 최대 500명)
    feedback.cpp  : 진동 3개 + 브레이크 서보
    ble_link.cpp  : 앱(WhiteCaneGuardian BleManager.kt) 통신
    main.cpp      : 판단 + 이벤트 전송 (코어 1)
    lib/STM32duino_VL53L4CX : I2C 무한 재시도 버그를 고친 VL53L4CX 라이브러리
*/
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
uint8_t sentObstacle = 0xFF;
uint32_t obstacleEventAt = 0;
bool stairAlert = false, sentStair = false;
bool yoloOn = false;
bool lastButton = HIGH, stableButton = HIGH;
uint32_t buttonAt = 0, printAt = 0, healthAt = 0;
bool mpuUsable = false, stairUsable = false, obstacleUsable = false;
bool prevMpuUsable = false, prevStairUsable = false, prevObstacleUsable = false;
float leftCm = -1, rightCm = -1;
String piBuffer;
bool piDiscard = false;
uint32_t piRxAt = 0;

// ---------- 손잡이 포고핀 상태 (앱 메시지: "포고핀 분리상태 " / "포고핀 재결합 ") ----------
enum class Pogo : uint8_t { Booting, Connected, Detached };
Pogo pogo = Pogo::Booting;
uint32_t partialAt = 0, mpuFaultAt = 0, stairFaultAt = 0;
bool mpuWarned = false, stairWarned = false;
constexpr uint32_t PARTIAL_REJOIN_MS = 3000;   // 한 센서만 돌아와도 3초 뒤 재결합으로 처리
constexpr uint32_t SENSOR_FAULT_REPORT_MS = 1500;   // 분리 과정의 순간적인 한쪽 끊김은 무시

void reportSensorFaults(bool mpu, bool stair, uint32_t now) {
  if (!mpu) {
    if (!mpuFaultAt) mpuFaultAt = now;
    if (!mpuWarned && nav::elapsed(now, mpuFaultAt, SENSOR_FAULT_REPORT_MS)) { mpuWarned = true; sendMessage("낙상센서 오류"); }
  } else {
    mpuFaultAt = 0;
    if (mpuWarned) { mpuWarned = false; sendMessage("낙상센서 복구"); }
  }
  if (!stair) {
    if (!stairFaultAt) stairFaultAt = now;
    if (!stairWarned && nav::elapsed(now, stairFaultAt, SENSOR_FAULT_REPORT_MS)) { stairWarned = true; sendMessage("계단센서 오류"); }
  } else {
    stairFaultAt = 0;
    if (stairWarned) { stairWarned = false; sendMessage("계단센서 복구"); }
  }
}

void updatePogo(bool mpu, bool stair, uint32_t now) {
  const bool any = mpu || stair, all = mpu && stair;
  switch (pogo) {
    case Pogo::Booting:   // 부팅 후 두 센서가 준비되거나 8초가 지나면 상태 확정
      if (all || now > 8000) pogo = any ? Pogo::Connected : Pogo::Detached;
      break;
    case Pogo::Connected:
      if (!any) {
        pogo = Pogo::Detached; partialAt = 0;
        mpuWarned = stairWarned = false; mpuFaultAt = stairFaultAt = 0;
        sendMessage("포고핀 분리상태 ");
      } else {
        reportSensorFaults(mpu, stair, now);
      }
      break;
    case Pogo::Detached:
      if (all) {
        pogo = Pogo::Connected; partialAt = 0;
        sendMessage("포고핀 재결합 ");
      } else if (any) {
        if (!partialAt) partialAt = now;
        else if (nav::elapsed(now, partialAt, PARTIAL_REJOIN_MS)) {
          pogo = Pogo::Connected; partialAt = 0;
          sendMessage("포고핀 재결합 ");
          mpuFaultAt = stairFaultAt = now - SENSOR_FAULT_REPORT_MS;   // 빠진 센서는 바로 알림
          reportSensorFaults(mpu, stair, now);
        }
      } else {
        partialAt = 0;
      }
      break;
  }
}

// LED 모듈 (켜짐 = 정상): Q0 장애물ToF, Q1 낙상MPU, Q2 계단ToF, Q3 앱 연결, Q4 YOLO 동작
uint8_t ledMask() {
  return (obstacleUsable ? 0x01 : 0) | (mpuUsable ? 0x02 : 0) | (stairUsable ? 0x04 : 0) |
         (bleConnected() ? 0x08 : 0) | (yoloOn ? 0x10 : 0);
}

void sendStatus() {
  sendMessage(String("STATUS:MPU=") + (mpuUsable ? "OK" : "FAULT"));
  sendMessage(String("STATUS:STAIR=") + (stairUsable ? "OK" : "FAULT"));
  sendMessage(String("STATUS:OBSTACLE=") + (obstacleUsable ? "OK" : "FAULT"));
  sendMessage(String("STATUS:MODULE=") + expansionModuleName() + (expansionFault() ? "(FAULT)" : ""));
  sendMessage("STATUS:ID_MV=" + String(expansionIdMv()));
  sendMessage("STATUS:PROFILE=" + String(activeProfileName) + "," + String(activeObstacleThresholdCM, 0) + "," +
              String(activeStairThresholdCM, 0) + "," + String(activeVibDuty));
  sendMessage(String("STATUS:YOLO=") + (yoloOn ? "ON" : "OFF"));
}

// 앱이 알림을 켠 직후: 현재 상태를 다시 맞춘다
void syncAfterConnect() {
  sendMessage("CONNECTED");
  if (pogo == Pogo::Detached) sendMessage("포고핀 분리상태 ");
  if (stairAlert) sendMessage("계단 감지 ");
  if (nav::isModule(expansionModule())) sendMessage(String(expansionModuleName()) + " 연결");
  float upperCm;
  if (expansionUpperObstacle(upperCm)) sendMessage("상부 장애물");
  sentObstacle = 0xFF;                     // 장애물 상태 다시 전송
  obstacleEventAt = millis() - 300;
}

void processCommand(const String &c) {
  Serial.println("[BLE 수신] " + c);
  if (profilesCommand(c)) return;   // CFG, REG, REG_CANCEL, SET_STAIR_CM
  if (c == "PING") sendMessage("PONG");
  else if (c == "GET_STATUS") sendStatus();
  else if (c == "GET_ACCEL") sendMessage("ACCEL:" + String(mpuUsable ? sensors.accel : -1, 2));
  else if (c == "GET_DISTANCE") {
    float cm = leftCm < 0 ? rightCm : rightCm < 0 ? leftCm : min(leftCm, rightCm);
    sendMessage("DISTANCE:" + String(cm, 1));
  }
  else if (c == "GET_STAIR_DISTANCE") sendMessage("STAIR_DISTANCE:" + String(stairUsable ? sensors.stairCm : -1, 1));
  else if (c == "GET_ULTRASONIC") sendMessage("ULTRASONIC:" + String(expansionUltrasonicCm(), 1));
  else if (c == "LED_TEST") expansionLedTest();
  else sendMessage("ERROR:UNKNOWN_COMMAND");
}

void checkButton() {
  const bool raw = digitalRead(cfg::BUTTON);
  const uint32_t now = millis();
  if (raw != lastButton) { lastButton = raw; buttonAt = now; }
  if (nav::elapsed(now, buttonAt, 50) && raw != stableButton) {
    stableButton = raw;
    if (raw == LOW) {
      Serial2.print("BTN\n");          // Pi5: 신호등 인식 시작
      sendMessage("BUTTON_PRESSED");   // 앱: 음성 안내 트리거 (먼저 보냄)
      expansionRemotePress();          // 리모컨 모듈이 꽂혀 있으면 음향신호기도 같이
    }
  }
}

void checkPi() {
  const uint32_t now = millis();
  if (piBuffer.length() && nav::elapsed(now, piRxAt, 1000)) { piBuffer = ""; piDiscard = false; }
  for (int n = 0; n < 64 && Serial2.available(); ++n) {
    char c = Serial2.read();
    piRxAt = now;
    if (c == '\r') continue;
    if (c == '\n') {
      if (!piDiscard) {
        if (piBuffer == "GREEN") { feedbackStart(Feedback::Green); sendMessage("GREEN_LIGHT"); }
        else if (piBuffer == "YOLO_ON") yoloOn = true;
        else if (piBuffer == "YOLO_OFF") yoloOn = false;
      }
      piBuffer = ""; piDiscard = false;
    } else if (!piDiscard) {
      if ((c >= 'A' && c <= 'Z') || c == '_') {
        piBuffer += c;
        if (piBuffer.length() > 24) { piBuffer = ""; piDiscard = true; }
      } else {
        piBuffer = ""; piDiscard = true;
      }
    }
  }
}

void consumeSensors() {
  sensors = sensorSnapshot();
  const uint32_t now = millis();
  mpuUsable = sensors.mpuReady && sensors.accelSeq && !nav::elapsed(now, sensors.accelAt, cfg::ACCEL_STALE_MS);
  stairUsable = sensors.stairReady && sensors.stairSeq && sensors.stairAt &&
                !nav::elapsed(now, sensors.stairAt, cfg::STAIR_STALE_MS);
  obstacleUsable = sensors.obstacleReady && sensors.obstacleSeq &&
                   !nav::elapsed(now, sensors.obstacleAt, cfg::OBSTACLE_STALE_MS);

  // 낙상
  if (sensors.fallCount != lastFallCount) {
    lastFallCount = sensors.fallCount;
    Serial.println(">>> 낙상 감지 <<<");
    sendMessage("FALL_DETECTED");
    feedbackStart(Feedback::Fall);
  }

  // 계단
  static float previousThreshold = activeStairThresholdCM;
  if (previousThreshold != activeStairThresholdCM) { previousThreshold = activeStairThresholdCM; stairDecision.reset(); }
  if (stairUsable && sensors.stairSeq != lastStairSeq) {
    lastStairSeq = sensors.stairSeq;
    stairDecision.sample(sensors.stairCm, sensors.stairNoTarget, activeStairThresholdCM);
  }
  if (!stairUsable) stairDecision.reset();   // 센서가 없으면 경고/브레이크 해제
  stairAlert = stairDecision.alert;
  if (stairAlert != sentStair) {
    sentStair = stairAlert;
    sendMessage(stairAlert ? "계단 감지 " : "STAIR_CLEARED");
  }

  // 하부 장애물 (좌/우)
  leftCm = obstacleUsable ? sensors.leftCm : -1;
  rightCm = obstacleUsable ? sensors.rightCm : -1;
  const bool left = leftCm >= 0 && leftCm < activeObstacleThresholdCM;
  const bool right = rightCm >= 0 && rightCm < activeObstacleThresholdCM;
  const uint8_t side = (left ? 1 : 0) | (right ? 2 : 0);
  if (side != sentObstacle && nav::elapsed(now, obstacleEventAt, 300)) {   // 경계 떨림 notify 폭주 방지
    sentObstacle = side; obstacleEventAt = now;
    static const char *names[] = {"OBSTACLE:NONE", "OBSTACLE:LEFT", "OBSTACLE:RIGHT", "OBSTACLE:BOTH"};
    sendMessage(names[side]);
  }

  // 센서 상태 (500ms)
  if (nav::elapsed(now, healthAt, 500)) {
    healthAt = now;
    updatePogo(sensors.mpuReady, sensors.stairReady, now);
    const bool lost = (prevObstacleUsable && !obstacleUsable) || (prevMpuUsable && !mpuUsable) ||
                      (prevStairUsable && !stairUsable);
    if (lost && pogo != Pogo::Booting) feedbackStart(Feedback::Fault);
    if (prevObstacleUsable != obstacleUsable) sendMessage(obstacleUsable ? "장애물센서 복구" : "장애물센서 오류");
    prevObstacleUsable = obstacleUsable; prevMpuUsable = mpuUsable; prevStairUsable = stairUsable;
  }

  if (nav::elapsed(now, printAt, 1000)) {
    printAt = now;
    float upperCm = -1;
    expansionUpperObstacle(upperCm);
    Serial.printf("[DATA] MPU=%s a=%.2f | 계단=%s %.1fcm 목표없음=%d 경고=%d | 장애물=%s L=%.1f R=%.1f | "
                  "슬롯=%s(%umV) 상부=%.1f | 프로필=%s | heap=%u\n",
                  mpuUsable ? "OK" : "X", sensors.accel, stairUsable ? "OK" : "X", sensors.stairCm,
                  sensors.stairNoTarget, stairAlert, obstacleUsable ? "OK" : "X", leftCm, rightCm,
                  expansionModuleName(), expansionIdMv(), expansionUltrasonicCm(), activeProfileName,
                  ESP.getFreeHeap());
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n동행 지팡이(NAVCANE) 펌웨어 시작");
  expansionBegin();   // 슬롯 신호선을 먼저 고임피던스로
  pinMode(cfg::BUTTON, INPUT_PULLUP);
  lastButton = stableButton = digitalRead(cfg::BUTTON);
  Serial2.begin(115200, SERIAL_8N1, cfg::UART_RX, cfg::UART_TX);
  feedbackBegin();
  profilesBegin();
  bleBegin();
  if (!sensorsBegin()) { Serial.println("[SYSTEM] 센서 태스크 생성 실패"); feedbackStart(Feedback::Fault); }
}

void loop() {
  consumeSensors();
  expansionTick(ledMask());   // 버튼/명령보다 먼저 ID 확인
  checkButton();
  checkPi();
  String command;
  for (int n = 0; n < 4 && blePopCommand(command); ++n) processCommand(command);
  profilesTick();
  if (bleTakeSync()) syncAfterConnect();
  float upperCm = -1;
  const bool upper = expansionUpperObstacle(upperCm);
  feedbackTick(stairAlert, leftCm, rightCm, upper, upperCm);
  brakeSet(stairAlert);
  bleTick();
  delay(2);
}
```

---

## lib/STM32duino_VL53L4CX/src/vl53l4cx_class.cpp (공식 1.1.0 대비 수정)

# NAVCANE 수정 사항 (STM32duino VL53L4CX 1.1.0 기반)

원본: https://github.com/stm32duino/VL53L4CX (BSD 3-Clause, LICENSE.md 유지)

수정 파일: src/vl53l4cx_class.cpp
- VL53L4CX_I2CRead: 주소 전송 실패 시 무한 재시도(do-while) -> 최대 3회 후 오류 반환.
  분할 읽기에서 requestFrom이 0을 반환하면 무한 루프가 되던 문제 -> 바이트 수 확인 후 오류 반환.
  여러 청크 읽기 시 각 청크마다 레지스터 주소를 다시 지정.
- VL53L4CX_I2CWrite: write()/endTransmission() 결과를 모두 확인해 실패 시 오류 반환.

이유: 손잡이 포고핀이 측정 중 분리되면 원본 라이브러리는 I2C 읽기에서 영원히 멈춰
같은 태스크의 MPU 낙상 감지까지 정지시킨다.

```diff
--- VL53L4CX-main/src/vl53l4cx_class.cpp	2022-11-24 13:43:57.000000000 +0000
+++ NAVCANE_FINAL_v2/lib/STM32duino_VL53L4CX/src/vl53l4cx_class.cpp	2026-10-02 06:21:13.206792032 +0000
@@ -128,84 +128,63 @@
 
 VL53L4CX_Error VL53L4CX::VL53L4CX_I2CWrite(uint8_t DeviceAddr, uint16_t RegisterAddr, uint8_t *pBuffer, uint16_t NumByteToWrite)
 {
+  // [NAVCANE PATCH] 모든 전송 결과를 확인하고 실패 시 즉시 오류 반환 (포고핀 분리 대비)
   uint32_t i = 0;
   uint8_t buffer[2];
 
   while (i < NumByteToWrite) {
-    // If still more than DEFAULT_I2C_BUFFER_LEN bytes to go, DEFAULT_I2C_BUFFER_LEN,
-    // else the remaining number of bytes
     size_t current_write_size = (NumByteToWrite - i > DEFAULT_I2C_BUFFER_LEN ? DEFAULT_I2C_BUFFER_LEN : NumByteToWrite - i);
 
     dev_i2c->beginTransmission((uint8_t)((DeviceAddr >> 1) & 0x7F));
-
-    // Target register address for transfer
     buffer[0] = (uint8_t)((RegisterAddr + i) >> 8);
     buffer[1] = (uint8_t)((RegisterAddr + i) & 0xFF);
-    dev_i2c->write(buffer, 2);
-    if (dev_i2c->write(pBuffer + i, current_write_size) == 0) {
+    if (dev_i2c->write(buffer, 2) != 2) {
+      dev_i2c->endTransmission(true);
+      return 1;
+    }
+    if (dev_i2c->write(pBuffer + i, current_write_size) != current_write_size) {
+      dev_i2c->endTransmission(true);
+      return 1;
+    }
+    if (dev_i2c->endTransmission(true) != 0) {
       return 1;
-    } else {
-      i += current_write_size;
-      if (NumByteToWrite - i) {
-        // Flush buffer and send stop bit so we have compatibility also with ESP32 platforms
-        dev_i2c->endTransmission(true);
-      }
     }
+    i += current_write_size;
   }
-
-  return dev_i2c->endTransmission(true);
+  return 0;
 }
 
 VL53L4CX_Error VL53L4CX::VL53L4CX_I2CRead(uint8_t DeviceAddr, uint16_t RegisterAddr, uint8_t *pBuffer, uint16_t NumByteToRead)
 {
-  int status = 0;
-  uint8_t buffer[2];
-
-  // Loop until the port is transmitted correctly
-  do {
-    dev_i2c->beginTransmission((uint8_t)((DeviceAddr >> 1) & 0x7F));
-
-    // Target register address for transfer
-    buffer[0] = (uint8_t)(RegisterAddr >> 8);
-    buffer[1] = (uint8_t)(RegisterAddr & 0xFF);
-    dev_i2c->write(buffer, 2);
-
-    status = dev_i2c->endTransmission(false);
-
-    // Fix for some STM32 boards
-    // Reinitialize the i2c bus with the default parameters
-#ifdef ARDUINO_ARCH_STM32
-    if (status) {
-      dev_i2c->end();
-      dev_i2c->begin();
-    }
-#endif
-    // End of fix
-
-  } while (status != 0);
-
+  // [NAVCANE PATCH] 원본은 주소 전송이 성공할 때까지 무한 재시도했다(센서 분리 시 태스크 정지).
+  // 최대 3회만 시도하고, 읽기 바이트가 모자라면 오류를 반환한다.
+  const uint8_t addr7 = (uint8_t)((DeviceAddr >> 1) & 0x7F);
   uint32_t i = 0;
-  if (NumByteToRead > DEFAULT_I2C_BUFFER_LEN) {
-    while (i < NumByteToRead) {
-      // If still more than DEFAULT_I2C_BUFFER_LEN bytes to go, DEFAULT_I2C_BUFFER_LEN,
-      // else the remaining number of bytes
-      uint8_t current_read_size = (NumByteToRead - i > DEFAULT_I2C_BUFFER_LEN ? DEFAULT_I2C_BUFFER_LEN : NumByteToRead - i);
-      dev_i2c->requestFrom(((uint8_t)((DeviceAddr >> 1) & 0x7F)),
-                           current_read_size);
+  while (i < NumByteToRead) {
+    const uint16_t reg = (uint16_t)(RegisterAddr + i);
+    int status = 1;
+    for (uint8_t attempt = 0; attempt < 3 && status != 0; attempt++) {
+      dev_i2c->beginTransmission(addr7);
+      uint8_t buffer[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
+      dev_i2c->write(buffer, 2);
+      status = dev_i2c->endTransmission(false);
+    }
+    if (status != 0) {
+      return 1;
+    }
+    uint8_t current_read_size = (NumByteToRead - i > DEFAULT_I2C_BUFFER_LEN ? DEFAULT_I2C_BUFFER_LEN : NumByteToRead - i);
+    uint8_t received = dev_i2c->requestFrom(addr7, current_read_size);
+    if (received != current_read_size) {
       while (dev_i2c->available()) {
-        pBuffer[i] = dev_i2c->read();
-        i++;
+        dev_i2c->read();
       }
+      return 1;
     }
-  } else {
-    dev_i2c->requestFrom(((uint8_t)((DeviceAddr >> 1) & 0x7F)), (uint8_t)NumByteToRead);
-    while (dev_i2c->available()) {
-      pBuffer[i] = dev_i2c->read();
-      i++;
+    for (uint8_t n = 0; n < current_read_size; n++) {
+      pBuffer[i++] = dev_i2c->read();
     }
   }
-
-  return i != NumByteToRead;
+  return 0;
 }
 
 
```