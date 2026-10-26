#pragma once
#include <Arduino.h>
enum class Feedback : uint8_t { Green, Applied, Unregistered, Registered, Failed, Fault, Fall };
void feedbackBegin();
void feedbackStart(Feedback pattern);
void feedbackTick(bool stairAlert, float leftCm, float rightCm, float frontCm);
void brakeSet(bool active);
