#pragma once
#include "Arduino.h"
#include <deque>
#include <vector>
#include <cassert>
struct FakeQueue { size_t capacity,itemSize; std::deque<std::vector<uint8_t>> entries; };
using QueueHandle_t=FakeQueue *;
constexpr int pdTRUE=1;
inline QueueHandle_t xQueueCreate(size_t n,size_t size) { return new FakeQueue{n,size,{}}; }
inline int xQueueSend(QueueHandle_t q,const void *p,int) { if(q->entries.size()==q->capacity) return 0; const auto *b=static_cast<const uint8_t *>(p); q->entries.emplace_back(b,b+q->itemSize); return pdTRUE; }
inline int xQueueReceive(QueueHandle_t q,void *p,int) { if(q->entries.empty()) return 0; memcpy(p,q->entries.front().data(),q->itemSize); q->entries.pop_front(); return pdTRUE; }
inline void xQueueReset(QueueHandle_t q) { q->entries.clear(); }
struct esp_ble_gatts_cb_param_t { struct { uint16_t mtu=23; } mtu; };
class BLEServer;
class BLEServerCallbacks { public: virtual ~BLEServerCallbacks()=default; virtual void onConnect(BLEServer *){} virtual void onDisconnect(BLEServer *){} virtual void onMtuChanged(BLEServer *,esp_ble_gatts_cb_param_t *){} };
class BLEDescriptor;
class BLEDescriptorCallbacks { public: virtual ~BLEDescriptorCallbacks()=default; virtual void onWrite(BLEDescriptor *){} };
class BLEDescriptor { public: BLEDescriptorCallbacks *callbacks=nullptr; void setCallbacks(BLEDescriptorCallbacks *c) { callbacks=c; } };
class BLE2902 : public BLEDescriptor { bool enabled=false; public: bool getNotifications() { return enabled; } void setNotifications(bool b) { enabled=b; } };
class BLECharacteristic;
class BLECharacteristicCallbacks { public: virtual ~BLECharacteristicCallbacks()=default; virtual void onWrite(BLECharacteristic *){} };
inline std::vector<std::string> fakeNotifications;
inline uint16_t fakePeerMtu=23;
class BLECharacteristic {
 public:
  static constexpr int PROPERTY_READ=1, PROPERTY_WRITE=2, PROPERTY_WRITE_NR=4, PROPERTY_NOTIFY=8;
  BLECharacteristicCallbacks *callbacks=nullptr;
  BLEDescriptor *descriptor=nullptr;
  std::string value;
  void setCallbacks(BLECharacteristicCallbacks *c) { callbacks=c; }
  void addDescriptor(BLEDescriptor *d) { descriptor=d; }
  void setValue(const char *s) { value=s; }
  void setValue(uint8_t *p,size_t n) { value.assign(reinterpret_cast<char *>(p),n); }
  std::string getValue() { return value; }
  void notify() { assert(value.size()<=size_t(fakePeerMtu-3)); fakeNotifications.push_back(value); }
};
class BLEService { public: BLECharacteristic characteristic; BLECharacteristic *createCharacteristic(const char *,int) { return &characteristic; } void start(){} };
class BLEAdvertising { public: int starts=0; void addServiceUUID(const char *){} void setScanResponse(bool){} void start(){ ++starts; } };
class BLEServer { public: BLEServerCallbacks *callbacks=nullptr; BLEService service; void setCallbacks(BLEServerCallbacks *c) { callbacks=c; } BLEService *createService(const char *) { return &service; } void startAdvertising(){} };
class BLEDevice { public: static inline BLEServer server; static inline BLEAdvertising advertising; static void init(const char *){} static void setMTU(int){} static BLEServer *createServer() { return &server; } static BLEAdvertising *getAdvertising() { return &advertising; } };
