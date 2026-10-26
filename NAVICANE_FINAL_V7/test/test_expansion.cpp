#include <cassert>
#include <vector>
#include <string>
#include "profiles.h"
#include "feedback.h"
std::vector<std::string> expansionMessages;
int registrationCancels=0;
void sendMessage(const String &s) { expansionMessages.emplace_back(s.c_str()); }
void feedbackStart(Feedback) {}
bool profilesWaiting() { return false; }
void profilesCard(const uint8_t *,uint8_t) {}
void profilesCancelRegistration(const char *) { ++registrationCancels; }
#include "../src/expansion.cpp"
int main() {
  expansionBegin(); fakeAdcMv=1650;
  fakeMillis=10; expansionTick(0); fakeMillis=310; expansionTick(0);
  assert(expansionModule()==nav::Module::Remote);
  fakeMillis=400; recordEcho(nav::EchoResult::Distance,6400,fakeMillis);
  fakeMillis=500; recordEcho(nav::EchoResult::Distance,6000,fakeMillis);
  assert(expansionUpperAlert()); float held=expansionUpperDistance(); assert(held>0);
  fakeMillis=600; recordEcho(nav::EchoResult::Far,38000,fakeMillis);
  assert(expansionUltrasonicCm()==-1 && expansionUpperAlert() && expansionUpperDistance()==held);
  fakeMillis=700; recordEcho(nav::EchoResult::NoResponse,0,fakeMillis);
  assert(expansionUpperAlert() && expansionUpperDistance()==held);
  fakeMillis=800; recordEcho(nav::EchoResult::Far,38000,fakeMillis);
  assert(expansionUpperAlert()); // invalid sample interrupted consecutive clearing
  fakeMillis=900; recordEcho(nav::EchoResult::Far,38000,fakeMillis);
  assert(!expansionUpperAlert() && expansionUpperDistance()==-1 && !expansionFault());
  fakeMillis=1000; recordEcho(nav::EchoResult::Distance,6000,fakeMillis);
  fakeMillis=1100; recordEcho(nav::EchoResult::Distance,6000,fakeMillis);
  for(int i=1;i<=5;++i) { fakeMillis=1100+i*100; recordEcho(nav::EchoResult::StuckHigh,0,fakeMillis); }
  assert(expansionUpperAlert());
  fakeMillis=1700; recordEcho(nav::EchoResult::StuckHigh,0,fakeMillis);
  assert(!expansionUpperAlert()); // invalid samples cannot keep stale haptics alive
  for(int i=0;i<4;++i) recordEcho(nav::EchoResult::StuckHigh,0,fakeMillis);
  assert(expansionFault());
  recordEcho(nav::EchoResult::Far,38000,fakeMillis);
  assert(!expansionFault() && std::string(expansionEchoState())=="FAR");
  // A one-poll ID glitch makes all shared pins input without resetting a live RC522.
  gate=nav::SlotGate{}; observed=nav::Module::None; highZ();
  fakeAdcMv=595; fakeMillis=2000; expansionTick(0); fakeMillis=2300; expansionTick(0);
  fakeMillis=2360; expansionTick(0); assert(expansionRfidReady());
  int resets=rfid.resets, cancels=registrationCancels;
  fakeAdcMv=0; fakeMillis=2365; expansionTick(0);
  assert(!expansionRfidReady() && expansionModule()==nav::Module::Rfid);
  assert(fakePinModes[cfg::S1]==INPUT && fakePinModes[cfg::S2]==INPUT && fakePinModes[cfg::S4]==INPUT);
  fakeAdcMv=595; fakeMillis=2370; expansionTick(0); fakeMillis=2400; expansionTick(0);
  assert(expansionRfidReady() && rfid.resets==resets && registrationCancels==cancels);
  fakeAdcMv=3300; fakeMillis=2410; expansionTick(0); fakeMillis=2510; expansionTick(0);
  assert(expansionModule()==nav::Module::None && registrationCancels==cancels+1);
  puts("expansion: confirmed upper clearing, held warning distance, stale/fault release, far recovery, slot high-Z and reset-free resume PASS");
}
