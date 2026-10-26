#pragma once
#include <Arduino.h>
extern float activeObstacleThresholdCM;
extern float activeStairThresholdCM;
extern uint32_t activeVibDuty;
extern char activeProfileName[13];
void profilesBegin();
bool profilesCommand(const String &command);
void profilesCard(const uint8_t *uid, uint8_t length);
void profilesTick();
void profilesCancelRegistration(const char *reason);
bool profilesWaiting();

bool profilesStoreReady();
void profilesClearPending();
void profilesSerialCommand(const String &command);
