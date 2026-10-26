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
