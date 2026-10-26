#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include "sensors.h"
#include "profiles.h"
#include "expansion.h"
#include "feedback.h"
SensorSnapshot fakeSnapshot;
bool fakeExpansionFault=false;
int faultStarts=0;
std::vector<std::string> mainMessages;
float activeObstacleThresholdCM=50, activeStairThresholdCM=170;
uint32_t activeVibDuty=255;
char activeProfileName[13]="DEFAULT";
SensorSnapshot sensorSnapshot() { return fakeSnapshot; }
bool expansionFault() { return fakeExpansionFault; }
uint16_t expansionIdMv() { return 3300; }
nav::Module expansionModule() { return nav::Module::None; }
bool expansionUpperAlert() { return false; }
const char *expansionEchoState() { return "ABSENT"; }
bool profilesStoreReady() { return true; }
bool bleConnected() { return true; }
void sendMessage(const String &s) { mainMessages.emplace_back(s.c_str()); }
void feedbackStart(Feedback f) { if(f==Feedback::Fault) ++faultStarts; }
#include "../src/main.cpp"
void fresh() {
  fakeSnapshot.mpuReady=fakeSnapshot.stairReady=fakeSnapshot.obstacleReady=true;
  fakeSnapshot.accelSeq=fakeSnapshot.obstacleSeq=1;
  if(!fakeSnapshot.stairSeq) fakeSnapshot.stairSeq=1;
  fakeSnapshot.accelAt=fakeSnapshot.stairAt=fakeSnapshot.obstacleAt=fakeMillis;
  fakeSnapshot.stairCm=150; fakeSnapshot.leftCm=10; fakeSnapshot.rightCm=20;
}
size_t occurrences(const char *line) {
  return std::count(mainMessages.begin(),mainMessages.end(),line);
}
int main() {
  fakeMillis=1000; fresh(); previousHealth=0; consumeSensors();
  fakeMillis+=501; fresh(); fakeExpansionFault=true; consumeSensors();
  assert(faultStarts==0); // expansion.cpp already owns its warning, do not restart it
  fakeMillis+=501; fresh(); fakeExpansionFault=false; consumeSensors();
  fakeMillis+=501; fresh(); fakeSnapshot.obstacleReady=false; consumeSensors();
  assert(faultStarts==1); // an actual new base sensor fault still warns
  fakeMillis+=501; fresh(); fakeSnapshot.obstacleReady=false; consumeSensors();
  assert(faultStarts==1);
  fakeMillis+=501; fresh(); fakeSnapshot.obstacleAt=fakeMillis-599;
  consumeSensors(); assert(obstacleUsable && leftCm==10 && rightCm==20);
  ++fakeMillis; fakeSnapshot.accelAt=fakeSnapshot.stairAt=fakeMillis;
  consumeSensors(); assert(!obstacleUsable && leftCm==-1 && rightCm==-1);
  // The driver remains ready until its separate 2500ms watchdog. Main ignores stale data.
  assert(fakeSnapshot.obstacleReady);
  fakeMillis+=20; fresh(); fakeSnapshot.stairCm=180; ++fakeSnapshot.stairSeq;
  consumeSensors(); assert(!stairAlert);
  fakeMillis+=20; fresh(); fakeSnapshot.stairCm=180; ++fakeSnapshot.stairSeq;
  consumeSensors(); assert(stairAlert && occurrences("계단 감지")==1);
  fakeMillis+=20; fresh(); fakeSnapshot.stairCm=180; ++fakeSnapshot.stairSeq;
  consumeSensors(); assert(occurrences("계단 감지")==1 && occurrences("STAIR_DETECTED")==0);
  status(); assert(occurrences("계단 감지")==2); // explicit state synchronization
  for(int i=0;i<3;++i) { fakeMillis+=20; fresh(); ++fakeSnapshot.stairSeq; consumeSensors(); }
  assert(!stairAlert && occurrences("STAIR_CLEARED")==1);
  fakeSnapshot.mpuReady=true; fakeSnapshot.accelSeq=1;
  fakeSnapshot.accelAt=fakeMillis-120; sensors=fakeSnapshot; mpuUsable=false;
  assert(std::string(mpuStatusText()).find("측정값 갱신 지연")!=std::string::npos);
  fakeSnapshot.mpuReady=false; fakeSnapshot.mpuReason="I2C 응답 부족"; sensors=fakeSnapshot;
  assert(std::string(mpuStatusText())=="I2C 응답 부족");
  fakeSnapshot.stairReady=false; fakeSnapshot.stairReason="미연결 또는 모델 ID 읽기 실패"; stairUsable=false;
  sensors=fakeSnapshot;
  assert(std::string(stairStatusText()).find("미연결")!=std::string::npos);
  puts("main: no duplicate expansion fault, base sensor warning, 599/600ms freshness, Korean stair events and status sync PASS");
}
