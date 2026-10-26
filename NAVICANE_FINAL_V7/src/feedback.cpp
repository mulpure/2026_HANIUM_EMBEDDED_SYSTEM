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
