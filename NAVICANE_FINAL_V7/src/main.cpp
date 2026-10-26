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
