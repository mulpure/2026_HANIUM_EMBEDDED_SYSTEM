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
  return v == 0x91 || v == 0x92 || v == 0x88 || v == 0x90 || v == 0x12 || v == 0xB2 || v == 0x82; // 쉼표(,) 오타 수정
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
