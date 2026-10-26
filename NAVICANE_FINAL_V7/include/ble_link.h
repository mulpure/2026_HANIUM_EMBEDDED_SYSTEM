#pragma once
#include <Arduino.h>
void bleBegin();
void bleTick();
bool blePopCommand(String &command);
void sendMessage(const String &message);
void sendProtocolMessage(const String &message);
bool bleConnected();
uint32_t bleSessionEpoch();
bool bleTakeSync();

void bleEnableStream();
