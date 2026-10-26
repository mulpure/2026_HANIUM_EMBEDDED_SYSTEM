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
  Wire.begin(cfg::OBSTACLE_SDA,cfg::OBSTACLE_SCL,cfg::OBSTACLE_INIT_CLOCK_HZ);
  Wire.setTimeOut(cfg::OBSTACLE_I2C_TIMEOUT_MS);
  vTaskDelay(pdMS_TO_TICKS(50));
  digitalWrite(cfg::OBSTACLE_LPN, HIGH); vTaskDelay(pdMS_TO_TICKS(cfg::OBSTACLE_BOOT_MS));
  if (!present(Wire, 0x29)) { obstacleInitReason="주소 0x29 응답 없음: 18/19·LPn25·전원 확인"; return false; }
  obstacleInitReason="초기화 중: 센서 펌웨어 전송"; publishObstacle(false,-1,-1,millis(),0);
  Serial.println("[장애물 VL53L5CX] 주소 응답 확인. 센서 펌웨어 전송을 시작합니다.");
  // 펌웨어 전송은 100kHz로 끝까지 진행한다(이전 정상 버전과 동일).
  if (!obstacle.begin(0x29, Wire)) {
    Serial.printf("[장애물 VL53L5CX] begin 실패: 0x29 응답=%s, 오류코드=%u, 오류값=%lu\n",
      present(Wire,0x29) ? "있음" : "없음",
      static_cast<unsigned>(obstacle.lastError.lastErrorCode),
      static_cast<unsigned long>(obstacle.lastError.lastErrorValue));
    obstacleInitReason="센서 펌웨어 전송 또는 초기화 실패"; return false;
  }
  Wire.setClock(cfg::OBSTACLE_RUN_CLOCK_HZ);
  if (!obstacle.setResolution(64)) { obstacleInitReason="64구역 측정 설정 실패"; return false; }
  if (!obstacle.setRangingFrequency(15)) { obstacleInitReason="측정 주파수 설정 실패"; return false; }
  if (!obstacle.startRanging()) { obstacleInitReason="거리 측정 시작 실패"; return false; }
  obstacleInitReason="첫 측정 프레임 대기"; return true;
}
void obstacleTask(void *) {
  Wire.begin(cfg::OBSTACLE_SDA, cfg::OBSTACLE_SCL, cfg::OBSTACLE_INIT_CLOCK_HZ);
  Wire.setTimeOut(cfg::OBSTACLE_I2C_TIMEOUT_MS);
  nav::ObstacleRecovery recovery; nav::Mean3 leftMean, rightMean;
  uint32_t at = 0, seq = 0, ackAt = 0;
  uint8_t ackMiss = 0;
  for (;;) {
    uint32_t now = millis();
    if (recovery.shouldInitialize(now)) {
      bool ready = initObstacle(); at = millis(); recovery.initialized(ready,at);
      leftMean.reset(); rightMean.reset(); ackMiss = 0; ackAt = at;
      if (ready) Serial.println("[장애물 VL53L5CX] 초기화 완료. 첫 거리 측정을 기다립니다.");
      else Serial.println(String("[장애물 VL53L5CX] 초기화 실패: ")+obstacleInitReason+". 3초 뒤 재시도합니다.");
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
    // 이전 정상 버전과 같은 상태 점검: 500ms마다 0x29 응답, 연속 2회 미응답이면 재초기화.
    // 이 버스(18/19)는 이 태스크만 쓰므로 측정 읽기와 겹치지 않는다.
    if (recovery.running() && nav::elapsed(millis(),ackAt,cfg::OBSTACLE_ACK_MS)) {
      ackAt = millis();
      if (present(Wire,0x29)) ackMiss = 0;
      else if (++ackMiss >= cfg::OBSTACLE_ACK_MISS_LIMIT) {
        ackMiss = 0; recovery.lost();
        obstacleInitReason="주소 0x29 연속 미응답: 재초기화";
        Serial.println("[장애물 VL53L5CX] 0x29 응답이 연속 2회 없습니다. LPn 리셋 후 재초기화합니다.");
        leftMean.reset(); rightMean.reset();
        publishObstacle(false, -1, -1, at, seq);
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
  // VL53L5CX는 BLE 스택(코어 0)과 떨어진 코어 1에서 초기화·측정한다(이전 정상 버전은 loop=코어 1에서 동작).
  const auto c=xTaskCreatePinnedToCore(obstacleTask,"obstacle",8192,nullptr,1,&obstacleHandle,1);
  return a==pdPASS && b==pdPASS && c==pdPASS;
}
