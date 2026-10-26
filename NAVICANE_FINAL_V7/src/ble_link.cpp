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
// [임시] MODULE_LOG_ONLY일 때 시리얼에 찍을 확장모듈 관련 이벤트
static bool isModuleEvent(const String &s) {
  return s.startsWith("MODULE,") || s.startsWith("REMOTE,") || s.startsWith("LED,") ||
         s.startsWith("ERR,LED") || s.startsWith("SENSOR:ULTRASONIC") ||
         s.startsWith("ULTRASONIC:") || s.startsWith("OBSTACLE:UPPER") || s.startsWith("ID_MV:") ||
         s.startsWith("ACK,CFG") || s.startsWith("ERR,CFG") || s.startsWith("REG,") ||
         s.startsWith("APPLY,") || s.startsWith("UNREG,") ||
         s == "RFID 연결" || s == "리모컨 연결" || s == "LED 연결" || s == "확장모듈 분리";
}
void sendMessage(const String &s) {
  if (!cfg::MODULE_LOG_ONLY || isModuleEvent(s)) { // [임시] BLE 전송은 항상 유지
    Serial.println(nav::serialEventText(s));
    if (cfg::SERIAL_RAW_PROTOCOL) Serial.println("[통신 원문] " + s);
  }
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
