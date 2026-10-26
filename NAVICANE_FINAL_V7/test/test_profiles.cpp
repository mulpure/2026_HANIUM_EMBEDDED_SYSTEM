#include <cassert>
#include <vector>
#include <string>
#include "feedback.h"
#include "expansion.h"
std::vector<std::string> messages;
bool rfidReady=false;
bool fakeLinked=true;
uint32_t fakeSession=1;
bool bleConnected() { return fakeLinked; }
uint32_t bleSessionEpoch() { return fakeSession; }
void sendMessage(const String &s) { messages.emplace_back(s.c_str()); }
void sendProtocolMessage(const String &s) { sendMessage(s); }
void feedbackStart(Feedback) {}
bool expansionRfidReady() { return rfidReady; }
#include "../src/profiles.cpp"
static void expect(const char *line) { assert(!messages.empty() && messages.back()==line); }
int main() {
  LittleFS.mounted=false; profilesBegin(); assert(profileStoreReady && LittleFS.formats==1);
  profilesCommand("CFG,ALICE,80,17,1"); expect("ACK,CFG");
  assert(activeObstacleThresholdCM==50 && pendingCfg.valid);
  profilesCommand("CFG,BAD,201,17,1"); expect("ERR,CFG,RANGE"); assert(std::string(pendingCfg.name)=="ALICE");
  profilesCommand("CFG,A B,80,17,1"); expect("ERR,CFG,FORMAT");
  profilesCommand("REG"); expect("REG,NO_RFID");
  rfidReady=true; profilesCommand("REG"); expect("REG,WAIT,15");
  uint8_t uid[4]={1,2,3,4}; profilesCard(uid,4); expect("REG,OK,01020304,NEW,ALICE");
  assert(profileCount==1 && activeObstacleThresholdCM==80);
  auto durable=*LittleFS.files[PROFILE_FILE_PATH];
  profilesCommand("CFG,BOB,90,18,0"); assert(activeObstacleThresholdCM==80);
  writeBudget=5; profilesCommand("REG"); profilesCard(uid,4); expect("REG,FSERR");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==durable && std::string(profiles[0].name)=="ALICE");
  writeBudget=-1; renameFails=true; profilesCommand("REG"); profilesCard(uid,4); expect("REG,FSERR");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==durable && activeObstacleThresholdCM==80);
  renameFails=false; corruptTempRead=true; profilesCommand("REG"); profilesCard(uid,4); expect("REG,FSERR");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==durable);
  corruptTempRead=false; profilesCommand("REG"); profilesCard(uid,4); expect("REG,OK,01020304,UPDATE,BOB");
  assert(profileCount==1 && activeObstacleThresholdCM==90 && activeVibDuty==cfg::VIB_WEAK_DUTY);
  applyDefaultProfile(); loadProfiles(); assert(profileCount==1 && profileStoreReady);
  fakeMillis+=3000; profilesCard(uid,4); expect("APPLY,01020304,BOB");
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG"); profilesClearPending();
  assert(!profilesWaiting() && !pendingCfg.valid); expect("REG,CANCEL,DISCONNECTED");
  profilesCommand("REG"); expect("REG,NOCFG");
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG"); fakeMillis+=15000; profilesTick(); expect("REG,TIMEOUT");
  auto beforeDeadline=*LittleFS.files[PROFILE_FILE_PATH];
  profilesCommand("REG"); fakeMillis+=15000;
  profilesCard(uid,4); expect("REG,TIMEOUT");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==beforeDeadline);
  assert(std::string(profiles[0].name)=="BOB" && activeObstacleThresholdCM==90);
  auto beforeSession=*LittleFS.files[PROFILE_FILE_PATH];
  profilesCommand("REG"); ++fakeSession; // rapid reconnect, no loop cleanup yet
  profilesCard(uid,4); expect("REG,CANCEL,DISCONNECTED");
  assert(!pendingCfg.valid && *LittleFS.files[PROFILE_FILE_PATH]==beforeSession);
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG"); fakeLinked=false;
  profilesCard(uid,4); expect("REG,CANCEL,DISCONNECTED");
  assert(!pendingCfg.valid && *LittleFS.files[PROFILE_FILE_PATH]==beforeSession);
  fakeLinked=true; ++fakeSession;
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG");
  fakeMillis=0xFFFFFF00u; regStartTime=fakeMillis;
  fakeMillis=uint32_t(regStartTime+14999u); assert(profilesWaiting());
  fakeMillis=uint32_t(regStartTime+15000u); profilesCard(uid,4); expect("REG,TIMEOUT");
  assert(*LittleFS.files[PROFILE_FILE_PATH]==beforeSession);
  profilesCommand("CFG,CAROL,100,17,1"); profilesCommand("REG");
  profilesCommand("CFG,DAVE,110,17,1"); expect("ACK,CFG"); assert(!profilesWaiting());
  // CRC failure must keep the damaged file and refuse replacement/false UNREG.
  (*LittleFS.files[PROFILE_FILE_PATH])[12]^=1; auto corrupt=*LittleFS.files[PROFILE_FILE_PATH];
  loadProfiles(); assert(!profileStoreReady && *LittleFS.files[PROFILE_FILE_PATH]==corrupt);
  profilesCommand("REG"); expect("REG,FSERR"); profilesCard(uid,4); expect("REG,FSERR");
  int formats=LittleFS.formats;
  LittleFS.mounted=false; blankPartition=false; loadProfiles(); assert(!profileStoreReady && LittleFS.formats==formats);
  profilesSerialCommand("FS_INIT"); assert(LittleFS.formats==formats);
  profilesSerialCommand("FS_INIT_CONFIRM_ERASE_PROFILES"); assert(profileStoreReady && profileCount==0 && LittleFS.formats==formats+1);
  profilesSerialCommand("FS_INIT_CONFIRM_ERASE_PROFILES"); assert(LittleFS.formats==formats+1);
  puts("profiles: app CFG/REG/APPLY, errors, timeout, session cleanup, short write, CRC, rename failure, mount protection PASS");
}
