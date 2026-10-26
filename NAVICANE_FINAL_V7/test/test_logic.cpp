#include "logic.h"
#include <assert.h>
#include <stdio.h>
using nav::Module;
int main() {
  const uint8_t pins[] = {cfg::OBSTACLE_SDA,cfg::OBSTACLE_SCL,cfg::OBSTACLE_LPN,
    cfg::SEGMENT_SDA,cfg::SEGMENT_SCL,cfg::VIB_MAIN,cfg::VIB_LEFT,cfg::VIB_RIGHT,
    cfg::SERVO,cfg::BUTTON,cfg::UART_RX,cfg::UART_TX,cfg::SLOT_ID,cfg::S1,cfg::S2,cfg::S3,cfg::S4};
  for(size_t i=0;i<sizeof(pins);++i) for(size_t j=i+1;j<sizeof(pins);++j) assert(pins[i]!=pins[j]);
  assert(nav::identify(595)==Module::Rfid);
  assert(nav::identify(1650)==Module::Remote);
  assert(nav::identify(2533)==Module::Led);
  assert(nav::identify(3300)==Module::None);
  assert(nav::identify(0)==Module::Unknown);
  assert(nav::identify(1000)==Module::Unknown);
  nav::ObstacleRecovery recovery;
  assert(recovery.shouldInitialize(0));
  recovery.initialized(false,1000);
  assert(!recovery.shouldInitialize(3999) && recovery.shouldInitialize(4000));
  recovery.initialized(true,7000); recovery.frame(7100);
  assert(nav::elapsed(7700,7100,cfg::OBSTACLE_STALE_MS));
  assert(!recovery.needsRestart(7700) && recovery.running()); // do not reset for a 600ms gap
  recovery.frame(8000); assert(!recovery.needsRestart(10499));
  assert(recovery.needsRestart(10500) && recovery.shouldInitialize(10500));
  recovery.initialized(false,14000);
  assert(!recovery.shouldInitialize(16999) && recovery.shouldInitialize(17000));
  recovery.initialized(true,0xFFFFFF00u); recovery.frame(0xFFFFFFF0u);
  assert(!recovery.needsRestart(uint32_t(0xFFFFFFF0u+2499u)));
  assert(recovery.needsRestart(uint32_t(0xFFFFFFF0u+2500u)));
  assert(recovery.shouldInitialize(uint32_t(0xFFFFFFF0u+2500u)));
  recovery.initialized(false,0xFFFFFF00u);
  assert(!recovery.shouldInitialize(uint32_t(0xFFFFFF00u+2999u)));
  assert(recovery.shouldInitialize(uint32_t(0xFFFFFF00u+3000u)));
  recovery.initialized(true,100); recovery.lost();
  assert(!recovery.running() && recovery.shouldInitialize(101));
  nav::SlotGate gate;
  assert(!gate.update(Module::Rfid,10));
  assert(!gate.update(Module::Rfid,309));
  assert(gate.update(Module::Rfid,310) && gate.permitted);
  // A one-sample glitch inhibits outputs without detach or repeated RFID reset.
  assert(!gate.update(Module::Unknown,315));
  assert(gate.active==Module::Rfid && !gate.permitted);
  gate.update(Module::Rfid,320); assert(!gate.permitted);
  gate.update(Module::Rfid,349); assert(!gate.permitted);
  assert(!gate.update(Module::Rfid,350) && gate.permitted);
  assert(!gate.update(Module::Remote,400) && !gate.permitted);
  assert(!gate.update(Module::Remote,499));
  assert(gate.update(Module::Remote,500) && gate.active==Module::None);
  assert(!gate.update(Module::Remote,699));
  assert(gate.update(Module::Remote,700) && gate.active==Module::Remote);
  // Noise across several ID bands still detaches a persistently unsafe signal.
  gate.update(Module::None,705); gate.update(Module::Unknown,750);
  assert(gate.update(Module::Led,805) && gate.active==Module::None);
  nav::SlotGate wrap;
  wrap.update(Module::Led,0xFFFFFF00u);
  assert(!wrap.update(Module::Led,0x0000002Bu));
  assert(wrap.update(Module::Led,0x0000002Cu));
  wrap.update(Module::None,0xFFFFFFF0u);
  assert(!wrap.update(Module::None,0x00000053u));
  assert(wrap.update(Module::None,0x00000054u));
  nav::ReconnectGate reconnect;
  assert(!reconnect.update(true,0));
  assert(!reconnect.update(true,2999));
  for(int t=3000;t<3800;t+=200) assert(!reconnect.update(true,t));
  assert(reconnect.update(true,3800));
  assert(!reconnect.update(false,4000));
  assert(!reconnect.update(true,4100));
  assert(!reconnect.update(false,7000));
  assert(!reconnect.update(true,7100));
  for(int t=10100;t<10900;t+=200) assert(!reconnect.update(true,t));
  assert(reconnect.update(true,10900));
  using nav::EchoResult;
  assert(nav::classifyEcho(true,true,false,38000)==EchoResult::Far);
  assert(nav::classifyEcho(true,true,false,6000)==EchoResult::Distance);
  assert(nav::classifyEcho(false,false,false,0)==EchoResult::NoResponse);
  assert(nav::classifyEcho(false,true,true,0)==EchoResult::StuckHigh);
  assert(nav::classifyEcho(false,false,true,0)==EchoResult::StuckHigh);
  assert(nav::classifyEcho(true,true,false,10)==EchoResult::Invalid);
  assert(nav::classifyEcho(true,true,false,65000)==EchoResult::Invalid);
  nav::UpperDecision upper;
  upper.sample(119); assert(!upper.alert); upper.sample(110); assert(upper.alert);
  upper.sample(125); upper.sample(125); assert(upper.alert);
  upper.sample(131); assert(upper.alert); upper.sample(-1); assert(!upper.alert);
  upper.reset(); upper.sample(119); upper.sample(125); upper.sample(119);
  assert(!upper.alert); // middle band breaks consecutive detection
  upper.sample(110); assert(upper.alert && upper.warningCm==110);
  upper.sample(-1); assert(upper.alert && upper.warningCm==110);
  upper.invalid(); upper.sample(-1); assert(upper.alert && upper.warningCm==110);
  upper.sample(125); upper.sample(131); assert(upper.alert && upper.warningCm==125);
  upper.sample(125); upper.sample(131); assert(upper.alert); // clear must be consecutive too
  upper.sample(132); assert(!upper.alert && upper.warningCm==-1);
  upper.sample(NAN); assert(!upper.alert);
  assert(nav::normalLeds(true,true,true,true,true)==31);
  assert(nav::normalLeds(false,true,false,false,true)==18);
  assert(nav::normalLeds(false,false,false,false,false)==0);
  nav::Mean3 mean;
  assert(mean.sample(30)==30); assert(mean.sample(60)==45);
  assert(mean.sample(90)==60); assert(mean.sample(-1)==-1); assert(mean.sample(12)==12);
  nav::StairDecision stair;
  stair.sample(175,false,170); assert(!stair.alert);
  stair.sample(180,false,170); assert(stair.alert);
  for(int i=0;i<4;i++) stair.sample(168,false,170);
  assert(stair.alert); // hysteresis retains warning at boundary
  stair.sample(150,false,170); stair.sample(150,false,170); assert(stair.alert);
  stair.sample(150,false,170); assert(!stair.alert);
  stair.sample(-1,true,170); assert(!stair.alert);
  stair.sample(-1,true,170); assert(stair.alert);
  stair.reset(); assert(!stair.alert);
  stair.sample(-1,false,170); stair.sample(-1,false,170); assert(!stair.alert);
  stair.sample(-1,false,170); assert(stair.alert);
  const uint8_t pos[6] = {0,0,0,0,0x10,0};
  const uint8_t neg[6] = {0,0,0,0,0xF0,0};
  assert(fabsf(nav::accelMagnitude(pos)-9.80665f)<0.001f);
  assert(fabsf(nav::accelMagnitude(neg)-9.80665f)<0.001f);
  nav::FallDetector fall;
  assert(!fall.sample(2,100)); assert(!fall.sample(20,200));
  assert(!fall.sample(9.8,700)); assert(!fall.sample(9.8,1699)); assert(fall.sample(9.8,1700));
  fall.sample(2,2000); fall.sample(9.8,2601); assert(!fall.sample(20,2650));
  fall.sample(2,3000); fall.sample(20,3100); fall.reset(); assert(!fall.sample(9.8,5000));
  assert(!fall.sample(NAN,6000));
  nav::FallDetector late;
  late.sample(2,0); late.sample(20,601);
  late.sample(9.8,1101); assert(!late.sample(9.8,2101));
  late.reset(); late.sample(2,3000); late.sample(20,3600);
  late.sample(9.8,4100); assert(!late.sample(9.8,5100)); // exact deadline is expired
  late.reset(); late.sample(2,6000); late.sample(20,6599);
  late.sample(9.8,7099); assert(late.sample(9.8,8099)); // final allowed millisecond
  puts("logic: obstacle recovery/backoff/wrap, ID, detach, debounce, stair and fall PASS");
}
