#pragma once
#include "Arduino.h"
class MFRC522 {
 public:
  enum { CommandReg, VersionReg, TxASKReg, TxControlReg, TxModeReg, RxModeReg,
         ModWidthReg, TModeReg, TPrescalerReg, TReloadRegH, TReloadRegL, ModeReg };
  enum StatusCode { STATUS_OK, STATUS_COLLISION, STATUS_TIMEOUT };
  static constexpr uint8_t PCD_SoftReset=0x0F;
  uint8_t registers[16]={}; int resets=0;
  struct { uint8_t size=4, uidByte[10]={1,2,3,4}; } uid;
  MFRC522(uint8_t,uint8_t) { registers[VersionReg]=0x92; }
  void PCD_WriteRegister(uint8_t reg,uint8_t value) {
    registers[reg]=value; if(reg==CommandReg && value==PCD_SoftReset) ++resets;
  }
  uint8_t PCD_ReadRegister(uint8_t reg) { return registers[reg]; }
  void PCD_AntennaOn() { registers[TxControlReg]|=3; }
  StatusCode PICC_WakeupA(byte *,byte *) { return STATUS_TIMEOUT; }
  bool PICC_ReadCardSerial() { return false; }
  void PICC_HaltA() {}
  void PCD_StopCrypto1() {}
};
