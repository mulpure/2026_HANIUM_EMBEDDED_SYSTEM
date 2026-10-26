#include <cassert>
#include <string>
#include "../src/ble_link.cpp"
void tick(uint32_t delta=15) { fakeMillis+=delta; bleTick(); }
void write(const std::string &s) { characteristic->value=s; characteristic->callbacks->onWrite(characteristic); }
void connect(uint16_t peer=185) {
  server->callbacks->onConnect(server); fakePeerMtu=peer;
  esp_ble_gatts_cb_param_t param; param.mtu.mtu=peer; server->callbacks->onMtuChanged(server,&param);
  cccd->setNotifications(true); cccd->callbacks->onWrite(cccd);
}
int main() {
  bleBegin(); uint32_t firstSession=bleSessionEpoch();
  connect(); assert(bleSessionEpoch()!=firstSession); assert(bleTakeSync()); assert(!bleTakeSync());
  sendMessage("포고핀 분리상태"); tick();
  assert(fakeNotifications.back()=="포고핀 분리상태\n"); // complete UTF8, one notify
  sendMessage("SENSOR:MPU:FAULT"); tick();
  assert(fakeNotifications.back()=="SENSOR:MPU:FAULT\n");
  assert(std::string(nav::serialEventText("SENSOR:MPU:FAULT").c_str()).find("가속도 MPU6050")!=std::string::npos);
  assert(std::string(nav::serialEventText("REG,OK,AABB,UPDATE,ALICE").c_str()).find("ALICE (기존 카드 갱신)")!=std::string::npos);
  write("CFG,ALICE,80,"); write("17,1\nREG\n");
  String command; assert(blePopCommand(command) && command=="CFG,ALICE,80,17,1");
  assert(blePopCommand(command) && command=="REG"); assert(!blePopCommand(command));
  write(std::string(97,'X')+"\nPING\n"); tick();
  assert(fakeNotifications.back()=="ERR,COMMAND,TOO_LONG\n");
  assert(blePopCommand(command) && command=="PING");
  write("REG\n"); sendMessage("OLD_EVENT");
  uint32_t priorSession=bleSessionEpoch();
  server->callbacks->onDisconnect(server); assert(!bleConnected() && bleSessionEpoch()!=priorSession);
  connect(); tick();
  assert(!blePopCommand(command)); assert(fakeNotifications.back()!="OLD_EVENT\n");
  server->callbacks->onDisconnect(server); tick(); sendMessage("FALL_DETECTED");
  connect(23); tick(); assert(fakeNotifications.back()=="FALL_DETECTED\n");
  sendMessage("포고핀 분리상태"); tick(); assert(fakeNotifications.back()=="ERROR:MTU_LOW\n");
  sendMessage("FALL_DETECTED"); tick(); assert(fakeNotifications.back()=="FALL_DETECTED\n");
  esp_ble_gatts_cb_param_t param; fakePeerMtu=185; param.mtu.mtu=185; server->callbacks->onMtuChanged(server,&param);
  tick(); assert(fakeNotifications.back()=="포고핀 분리상태\n");
  server->callbacks->onDisconnect(server); tick(); connect(23); bleEnableStream();
  write("REG"); assert(!blePopCommand(command));
  write("_CANCEL\n"); assert(blePopCommand(command) && command=="REG_CANCEL");
  size_t before=fakeNotifications.size();
  const std::string message="포고핀 분리상태";
  sendMessage(message.c_str()); tick(); // first UTF8 fragment may end mid-character
  sendMessage("FALL_DETECTED"); tick(); tick();
  std::string stream;
  for(size_t i=before;i<fakeNotifications.size();++i) stream+=fakeNotifications[i];
  assert(stream==message+"\nFALL_DETECTED\n"); // FALL must not corrupt an in-progress frame
  server->callbacks->onDisconnect(server); tick();
  write("CFG,BAD,80,"); connect(); write("REG\n");
  assert(blePopCommand(command) && command=="REG"); // session discards unfinished input
  // Exactly 5 minutes is expired; the preceding millisecond is still deliverable.
  server->callbacks->onDisconnect(server); tick(); sendMessage("FALL_DETECTED");
  uint32_t eventAt=fakeMillis; fakeMillis=eventAt+cfg::FALL_PENDING_MAX_MS-1;
  connect(23); size_t recentStart=fakeNotifications.size(); bleTick();
  assert(fakeNotifications.size()==recentStart+1 && fakeNotifications.back()=="FALL_DETECTED\n");
  server->callbacks->onDisconnect(server); tick(); sendMessage("FALL_DETECTED");
  eventAt=fakeMillis; fakeMillis=eventAt+cfg::FALL_PENDING_MAX_MS;
  connect(23); size_t expiredStart=fakeNotifications.size(); bleTick(); tick();
  for(size_t i=expiredStart;i<fakeNotifications.size();++i) assert(fakeNotifications[i]!="FALL_DETECTED\n");
  assert(!fallPending);
  // Expire offline before a later reconnect or uint32_t clock rollover.
  server->callbacks->onDisconnect(server); tick(); fakeMillis=0xFFFFFF00u;
  sendMessage("FALL_DETECTED"); eventAt=fakeMillis;
  fakeMillis=uint32_t(eventAt+cfg::FALL_PENDING_MAX_MS-1); bleTick(); assert(fallPending);
  fakeMillis=uint32_t(eventAt+cfg::FALL_PENDING_MAX_MS); bleTick(); assert(!fallPending);
  connect(23); expiredStart=fakeNotifications.size(); tick();
  for(size_t i=expiredStart;i<fakeNotifications.size();++i) assert(fakeNotifications[i]!="FALL_DETECTED\n");
  // A newer fall replaces the old timestamp rather than inheriting its deadline.
  server->callbacks->onDisconnect(server); tick(); sendMessage("FALL_DETECTED");
  fakeMillis+=cfg::FALL_PENDING_MAX_MS-1; sendMessage("FALL_DETECTED"); eventAt=fakeMillis;
  fakeMillis=eventAt+1; bleTick(); assert(fallPending);
  connect(23); recentStart=fakeNotifications.size(); tick();
  assert(fakeNotifications.size()==recentStart+1 && fakeNotifications.back()=="FALL_DETECTED\n");
  // Connected but CCCD disabled: expire there too, and never corrupt a long frame.
  cccd->setNotifications(false); sendMessage("FALL_DETECTED");
  fakeMillis+=cfg::FALL_PENDING_MAX_MS; bleTick(); assert(!fallPending);
  cccd->setNotifications(true); bleEnableStream();
  sendMessage("포고핀 분리상태"); tick(); sendMessage("FALL_DETECTED");
  fakeMillis+=cfg::FALL_PENDING_MAX_MS; expiredStart=fakeNotifications.size(); bleTick(); tick();
  for(size_t i=expiredStart;i<fakeNotifications.size();++i) assert(fakeNotifications[i]!="FALL_DETECTED\n");
  puts("BLE: framing, fall priority, 5min expiry/offline/CCCD/wrap/replacement, epochs and input limits PASS");
}
