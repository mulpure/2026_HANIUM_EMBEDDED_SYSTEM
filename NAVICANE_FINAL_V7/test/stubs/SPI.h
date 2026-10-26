#pragma once
struct FakeSPI {
  int begins=0, ends=0;
  void begin(int,int,int,int) { ++begins; }
  void end() { ++ends; }
};
inline FakeSPI SPI;
