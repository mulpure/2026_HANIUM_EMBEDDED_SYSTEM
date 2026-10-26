#pragma once
#include <Arduino.h>
#include "logic.h"
void expansionBegin();
void expansionTick(uint8_t ledHealthMask);
void expansionRemotePress();
void expansionLedTest();
bool expansionRfidReady();
nav::Module expansionModule();
uint16_t expansionIdMv();
float expansionUltrasonicCm();
bool expansionFault();

bool expansionUpperAlert();
float expansionUpperDistance();
const char *expansionEchoState();
