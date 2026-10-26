#pragma once
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cctype>
template<typename T> T min(T a,T b) { return std::min(a,b); }
class String {
  std::string value;
 public:
  String() = default;
  String(const char *s):value(s ? s : ""){}
  String(const std::string &s):value(s){}
  String(int v):value(std::to_string(v)){}
  String(long v):value(std::to_string(v)){}
  String(unsigned int v):value(std::to_string(v)){}
  String(unsigned long v):value(std::to_string(v)){}
  String(float v,int digits=2) { char b[80]; snprintf(b,sizeof(b),"%.*f",digits,v); value=b; }
  unsigned int length() const { return value.size(); }
  const char *c_str() const { return value.c_str(); }
  bool startsWith(const char *s) const { return value.rfind(s,0)==0; }
  bool endsWith(const char *s) const { size_t n=strlen(s); return n<=value.size() && value.compare(value.size()-n,n,s)==0; }
  int indexOf(char c,int from=0) const { auto p=value.find(c,from); return p==std::string::npos?-1:int(p); }
  String substring(int from) const { return value.substr(from); }
  String substring(int from,int end) const { return value.substr(from,end-from); }
  long toInt() const { return std::stol(value); }
  char operator[](size_t i) const { return value[i]; }
  String &operator+=(char c) { value+=c; return *this; }
  String &operator+=(const String &s) { value+=s.value; return *this; }
  bool operator==(const String &s) const { return value==s.value; }
  bool operator!=(const String &s) const { return !(*this==s); }
  friend String operator+(const String &a,const String &b) { return a.value+b.value; }
  void trim() { auto a=value.find_first_not_of(" \r\n\t"); auto b=value.find_last_not_of(" \r\n\t"); value=a==std::string::npos?"":value.substr(a,b-a+1); }
  void toCharArray(char *out,size_t n) const { if(n) { std::strncpy(out,value.c_str(),n); out[n-1]=0; } }
};
inline uint32_t fakeMillis=0;
inline unsigned long millis() { return fakeMillis; }
struct FakeSerial {
  void begin(uint32_t,int=0,uint8_t=0,uint8_t=0) {}
  int available() { return 0; }
  int read() { return -1; }
  void print(const char *) {}
  void println(const String &) {}
  template<typename... Args> void printf(const char *,Args...) {}
};
inline FakeSerial Serial;
inline FakeSerial Serial2;
inline void delay(uint32_t) {}
constexpr int INPUT_PULLUP=4, SERIAL_8N1=0;
struct FakeESP { uint32_t getFreeHeap() { return 200000; } };
inline FakeESP ESP;
inline uint32_t fakeDuty[16]={};
inline void ledcWrite(uint8_t channel,uint32_t duty) { fakeDuty[channel]=duty; }
inline void ledcSetup(uint8_t,int,int) {}
inline void ledcAttachPin(uint8_t,uint8_t) {}
template<typename T> T constrain(T value,T lo,T hi) { return std::max(lo,std::min(value,hi)); }
inline long map(long v,long a,long b,long c,long d) { return (v-a)*(d-c)/(b-a)+c; }
using byte=uint8_t;
#define IRAM_ATTR
constexpr int LOW=0, HIGH=1, INPUT=0, OUTPUT=1, CHANGE=2, ADC_11db=3, MSBFIRST=1;
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL(portMUX_TYPE *) {}
inline void portENTER_CRITICAL_ISR(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL_ISR(portMUX_TYPE *) {}
inline int fakePinModes[40]={}, fakePins[40]={};
inline uint16_t fakeAdcMv=3300;
inline void pinMode(uint8_t pin,int mode) { fakePinModes[pin]=mode; }
inline void digitalWrite(uint8_t pin,int value) { fakePins[pin]=value; }
inline int digitalRead(uint8_t pin) { return fakePins[pin]; }
inline uint32_t micros() { return fakeMillis*1000u; }
inline void delayMicroseconds(uint32_t) {}
inline int digitalPinToInterrupt(uint8_t pin) { return pin; }
inline void attachInterrupt(int,void (*)(),int) {}
inline void detachInterrupt(int) {}
inline void analogSetPinAttenuation(uint8_t,int) {}
inline uint16_t analogReadMilliVolts(uint8_t) { return fakeAdcMv; }
inline void shiftOut(uint8_t,uint8_t,int,uint8_t) {}
