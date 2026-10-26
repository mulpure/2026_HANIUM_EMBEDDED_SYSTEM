#pragma once
#include "Arduino.h"
#include <map>
#include <vector>
#include <memory>
#define FILE_READ "r"
#define FILE_WRITE "w"
inline int writeBudget=-1;
inline bool renameFails=false, corruptTempRead=false;
class File {
  std::shared_ptr<std::vector<uint8_t>> bytes;
  size_t pos=0;
  bool temp=false;
 public:
  File() = default;
  explicit File(std::shared_ptr<std::vector<uint8_t>> data,bool isTemp=false):bytes(data),temp(isTemp){}
  explicit operator bool() const { return bool(bytes); }
  size_t size() const { return bytes ? bytes->size() : 0; }
  size_t read(uint8_t *out,size_t n) { n=std::min(n,size()-pos); for(size_t i=0;i<n;++i) out[i]=(*bytes)[pos++]; return n; }
  int read() { if(!available()) return -1; auto b=(*bytes)[pos++]; return corruptTempRead && temp && pos==13 ? b^1 : b; }
  size_t write(const uint8_t *data,size_t n) {
    if(writeBudget>=0) { n=std::min(n,size_t(writeBudget)); writeBudget-=n; }
    bytes->insert(bytes->end(),data,data+n); pos+=n; return n;
  }
  bool available() const { return bytes && pos<bytes->size(); }
  void seek(size_t offset) { pos=offset; }
  void flush() {}
  void close() {}
};
struct FakeFS {
  bool mounted=true;
  int formats=0;
  std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;
  bool begin(bool) { return mounted; }
  bool format() { ++formats; mounted=true; files.clear(); return true; }
  void end() { mounted=false; }
  bool exists(const char *p) { return files.count(p); }
  File open(const char *p,const char *mode) {
    if(std::string(mode)==FILE_WRITE) files[p]=std::make_shared<std::vector<uint8_t>>();
    if(!files.count(p)) return {};
    return File(files[p],std::string(p)=="/profiles.tmp");
  }
  bool rename(const char *from,const char *to) {
    if(renameFails || !files.count(from)) return false;
    files[to]=files[from]; files.erase(from); return true;
  }
};
inline FakeFS LittleFS;
