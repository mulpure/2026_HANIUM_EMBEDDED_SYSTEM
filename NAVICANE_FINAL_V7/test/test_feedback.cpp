#include <cassert>
#include "profiles.h"
#include "config.h"
float activeObstacleThresholdCM=50, activeStairThresholdCM=170;
uint32_t activeVibDuty=255;
char activeProfileName[13]="DEFAULT";
#include "../src/feedback.cpp"
int main() {
  feedbackBegin(); feedbackTick(false,-1,-1,-1);
  assert(fakeDuty[cfg::VIB_CH_MAIN]==0 && fakeDuty[cfg::VIB_CH_LEFT]==0);
  feedbackStart(Feedback::Fault); feedbackStart(Feedback::Fault);
  feedbackTick(false,-1,-1,-1);
  assert(pattern==nullptr && fakeDuty[cfg::VIB_CH_MAIN]==0);
  assert(fakeDuty[cfg::VIB_CH_LEFT]==0 && fakeDuty[cfg::VIB_CH_RIGHT]==0);
  feedbackStart(Feedback::Green); const uint32_t greenAt=at;
  fakeMillis+=50; feedbackStart(Feedback::Fault); feedbackTick(false,-1,-1,-1);
  assert(pattern==green && at==greenAt && fakeDuty[cfg::VIB_CH_MAIN]==255);
  feedbackTick(true,-1,-1,-1); feedbackTick(false,-1,-1,-1);
  feedbackTick(false,-1,-1,110);
  assert(fakeDuty[cfg::VIB_CH_LEFT]==255 && fakeDuty[cfg::VIB_CH_RIGHT]==255);
  assert(fakeDuty[cfg::VIB_CH_MAIN]==0);
  feedbackStart(Feedback::Green); feedbackTick(true,-1,-1,-1);
  assert(fakeDuty[cfg::VIB_CH_MAIN]==255 && pattern==nullptr);
  brakeSet(true); uint32_t appliedBrake=fakeDuty[cfg::SERVO_CHANNEL];
  feedbackStart(Feedback::Fall); feedbackTick(true,-1,-1,-1);
  assert(pattern==fall && fakeDuty[cfg::VIB_CH_MAIN]==255);
  feedbackStart(Feedback::Fault); assert(pattern==fall);
  fakeMillis+=700; feedbackTick(true,-1,-1,-1);
  assert(fakeDuty[cfg::VIB_CH_MAIN]==0 && fakeDuty[cfg::SERVO_CHANNEL]==appliedBrake);
  fakeMillis+=200; feedbackTick(true,-1,-1,-1); assert(fakeDuty[cfg::VIB_CH_MAIN]==255);
  fakeMillis+=700; feedbackTick(true,-1,-1,-1); assert(fakeDuty[cfg::VIB_CH_MAIN]==0);
  fakeMillis+=200; feedbackTick(true,-1,-1,-1); assert(fakeDuty[cfg::VIB_CH_MAIN]==255);
  fakeMillis+=700; feedbackTick(true,-1,-1,-1); assert(pattern==nullptr && fakeDuty[cfg::VIB_CH_MAIN]==255);
  feedbackTick(false,-1,-1,-1); assert(fakeDuty[cfg::VIB_CH_MAIN]==0);
  brakeSet(false); assert(fakeDuty[cfg::SERVO_CHANNEL]!=appliedBrake);
  puts("feedback: silent faults, uninterrupted green/fall, upper both-sides, stair caution, retained brake and pattern completion PASS");
}
