#!/usr/bin/env python3
"""Exercise production policy and the exact patched driver transport with fake Wire."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'main'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_main.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)

def transport(file, start, end):
    text = file.read_text()
    return text[text.index(start):text.index(end, text.index(start))]

st = transport(root/'lib/VL53L4CX/src/vl53l4cx_class.cpp',
               "extern \"C\" bool navSegmentLock();",
               'VL53L4CX_Error VL53L4CX::VL53L4CX_GetTickCount(')
sf = transport(root/'lib/SparkFun_VL53L5CX/src/SparkFun_VL53L5CX_IO.cpp',
               'uint8_t SparkFun_VL53L5CX_IO::writeMultipleBytes(',
               'uint8_t SparkFun_VL53L5CX_IO::readSingleByte(')
stub = r'''
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdio>
#define highByte(x) uint8_t((x)>>8)
#define lowByte(x) uint8_t(x)
static int locks=0, unlocks=0, nesting=0;
static bool lockAllowed=true;
extern "C" bool navSegmentLock() { if(!lockAllowed) return false; ++locks; assert(nesting++==0); return true; }
extern "C" void navSegmentUnlock() { ++unlocks; assert(--nesting==0); }
using VL53L4CX_Error = int;
constexpr int VL53L4CX_ERROR_NONE=0, VL53L4CX_ERROR_CONTROL_INTERFACE=-1;
class FakeWire {
 public:
  uint8_t memory[65536] = {};
  std::vector<uint8_t> tx;
  uint16_t reg=0; int remaining=0;
  bool nack=false, shortRead=false, shortWrite=false;
  int ends=0, requests=0;
  void beginTransmission(uint8_t) { tx.clear(); }
  size_t write(uint8_t b) { tx.push_back(b); return 1; }
  size_t write(uint8_t *b,size_t n) { tx.insert(tx.end(),b,b+n); return shortWrite?n-1:n; }
  int endTransmission(bool=true) {
    ++ends; if(nack) return 4;
    assert(tx.size()>=2); reg=(uint16_t(tx[0])<<8)|tx[1];
    for(size_t i=2;i<tx.size();++i) memory[reg++]=tx[i]; return 0;
  }
  uint8_t requestFrom(uint8_t,uint8_t n) { ++requests; remaining=shortRead?n-1:n; return remaining; }
  int available() { return remaining; }
  int read() { if(!remaining) return -1; --remaining; return memory[reg++]; }
};
class VL53L4CX {
 public:
  FakeWire *dev_i2c;
  explicit VL53L4CX(FakeWire *w):dev_i2c(w){}
  int VL53L4CX_I2CWrite(uint8_t,uint16_t,uint8_t*,uint16_t);
  int VL53L4CX_I2CRead(uint8_t,uint16_t,uint8_t*,uint16_t);
};
class SparkFun_VL53L5CX_IO {
 public:
  FakeWire *_i2cPort; uint8_t _address=0x29; uint16_t wireMaxPacketSize=32;
  explicit SparkFun_VL53L5CX_IO(FakeWire *w):_i2cPort(w){}
  uint8_t writeMultipleBytes(uint16_t,uint8_t*,uint16_t);
  uint8_t readMultipleBytes(uint16_t,uint8_t*,uint16_t);
};
'''
checks = r'''
int main() {
  FakeWire wire; VL53L4CX st(&wire); SparkFun_VL53L5CX_IO sf(&wire);
  uint8_t source[99], out[99]; for(int i=0;i<99;++i) source[i]=i;
  assert(!st.VL53L4CX_I2CWrite(0x52,0x1234,source,99));
  assert(!st.VL53L4CX_I2CRead(0x52,0x1234,out,99));
  assert(!memcmp(source,out,99)); assert(wire.requests==4);
  wire.nack=true; int before=wire.ends;
  assert(st.VL53L4CX_I2CRead(0x52,0x1234,out,99)!=0);
  assert(wire.ends-before==1); // failed ACK MUST return, no infinite retry
  assert(st.VL53L4CX_I2CWrite(0x52,0x1234,source,99)!=0);
  wire.nack=false; wire.shortRead=true;
  assert(st.VL53L4CX_I2CRead(0x52,0x1234,out,99)!=0);
  assert(sf.readMultipleBytes(0x1234,out,20)!=0); // 짧은 읽기는 실패로 보고
  wire.shortRead=false; wire.shortWrite=true;
  assert(st.VL53L4CX_I2CWrite(0x52,0x1234,source,20)!=0);
  wire.shortWrite=false;
  assert(!sf.writeMultipleBytes(0x1234,source,99));
  assert(!sf.readMultipleBytes(0x1234,out,99)); assert(!memcmp(source,out,99));
  wire.nack=true; assert(sf.readMultipleBytes(0x1234,out,99)!=0);
  assert(sf.writeMultipleBytes(0x1234,source,99)!=0);
  assert(locks==unlocks && nesting==0);
  lockAllowed=false; int beforeLock=wire.ends;
  assert(st.VL53L4CX_I2CRead(0x52,0x1234,out,99)!=0);
  assert(wire.ends==beforeLock && locks==unlocks && nesting==0);
  puts("driver transport: long transfers, NACK, short reads/writes PASS");
}
'''
with tempfile.TemporaryDirectory() as temp:
    temp = Path(temp)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(root/'include'),
                    str(root/'test/test_logic.cpp'),'-o',str(temp/'logic')],check=True)
    subprocess.run([str(temp/'logic')],check=True,timeout=5)
    source = temp/'transport.cpp'; source.write_text(stub+st+sf+checks)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Wno-misleading-indentation',
                    str(source),'-o',str(temp/'transport')],check=True)
    subprocess.run([str(temp/'transport')],check=True,timeout=5)

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'profiles'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_profiles.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'expansion'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_expansion.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'ble'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_ble.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)

with tempfile.TemporaryDirectory() as temp:
    binary = Path(temp)/'feedback'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),'-I',str(root/'include'),
                    str(root/'test/test_feedback.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)
