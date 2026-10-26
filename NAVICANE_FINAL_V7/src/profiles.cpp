#include "profiles.h"
#include "feedback.h"
#include "ble_link.h"
#include "expansion.h"
#include "config.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <ctype.h>
constexpr int PROFILE_MAX_COUNT = 500, PROFILE_NAME_MAX_LEN = 12, RFID_UID_MAX_LEN = 10;
constexpr uint16_t OBSTACLE_CM_MIN = 20, OBSTACLE_CM_MAX = 200;
constexpr uint8_t HEIGHT_CODE_MIN = 14, HEIGHT_CODE_MAX = 19, VIB_LEVEL_MAX = 1;
constexpr uint8_t DEFAULT_HEIGHT_CODE = cfg::DEFAULT_HEIGHT_CODE, DEFAULT_VIB_LEVEL = cfg::DEFAULT_VIB_LEVEL;
constexpr uint16_t DEFAULT_OBSTACLE_CM = cfg::DEFAULT_OBSTACLE_CM;
constexpr uint32_t VIB_DUTY_WEAK = cfg::VIB_WEAK_DUTY, VIB_DUTY_STRONG = cfg::VIB_STRONG_DUTY;
constexpr uint32_t REG_WAIT_MS = cfg::REG_WAIT_MS, RFID_SAME_CARD_COOLDOWN_MS = 2000;
constexpr char DEFAULT_PROFILE_NAME[] = "DEFAULT";
const auto &STAIR_THRESHOLD_BY_HEIGHT_CM = cfg::STAIR_CM_BY_HEIGHT;
float activeObstacleThresholdCM = cfg::DEFAULT_OBSTACLE_CM;
float activeStairThresholdCM = cfg::DEFAULT_STAIR_CM;
uint32_t activeVibDuty = cfg::VIB_STRONG_DUTY;
char activeProfileName[13] = "DEFAULT";

const char PROFILE_FILE_PATH[] = "/profiles.bin";

// 고정 길이 레코드 28바이트. 파일 내 위치 = 인덱스 x 28
struct __attribute__((packed)) ProfileRecord
{
  uint8_t uid[RFID_UID_MAX_LEN];
  uint8_t uidLen;
  char name[PROFILE_NAME_MAX_LEN + 1];
  uint16_t obstacleCm;
  uint8_t heightCode;
  uint8_t vib;
};
static_assert(sizeof(ProfileRecord) == 28, "ProfileRecord must be 28 bytes");

ProfileRecord profiles[PROFILE_MAX_COUNT];   // 부팅 시 전체 로드 (약 14KB RAM)
int profileCount = 0;
bool profileStoreReady = false;

struct PendingConfig
{
  bool valid;
  uint32_t session;
  char name[PROFILE_NAME_MAX_LEN + 1];
  uint16_t obstacleCm;
  uint8_t heightCode;
  uint8_t vib;
};
PendingConfig pendingCfg = {};
bool regWaiting = false;
unsigned long regStartTime = 0;

uint8_t lastHandledUid[RFID_UID_MAX_LEN] = {};
uint8_t lastHandledUidLen = 0;
unsigned long lastHandledTime = 0;
String uidToHex(const uint8_t *uid, uint8_t len)
{
  static const char HEX_CHARS[] = "0123456789ABCDEF";
  String out;
  for (uint8_t i = 0; i < len; i++)
  {
    out += HEX_CHARS[uid[i] >> 4];
    out += HEX_CHARS[uid[i] & 0x0F];
  }
  return out;
}

float stairThresholdForHeight(uint8_t heightCode)
{
  if (heightCode < HEIGHT_CODE_MIN || heightCode > HEIGHT_CODE_MAX)
    heightCode = DEFAULT_HEIGHT_CODE;
  return STAIR_THRESHOLD_BY_HEIGHT_CM[heightCode - HEIGHT_CODE_MIN];
}

void applyProfileValues(const char *name, uint16_t obstacleCm, uint8_t heightCode, uint8_t vib)
{
  activeObstacleThresholdCM = obstacleCm;
  activeStairThresholdCM = stairThresholdForHeight(heightCode);
  activeVibDuty = vib ? VIB_DUTY_STRONG : VIB_DUTY_WEAK;
  strncpy(activeProfileName, name, PROFILE_NAME_MAX_LEN);
  activeProfileName[PROFILE_NAME_MAX_LEN] = '\0';
  // main resets its stair filter when the threshold changes.
  Serial.println("[RFID] 프로필 적용: " + String(activeProfileName) +
                 " / 장애물 " + String(activeObstacleThresholdCM, 0) +
                 "cm / 계단 " + String(activeStairThresholdCM, 0) +
                 "cm / 진동 " + (vib ? "강" : "약"));
}

void applyDefaultProfile()
{
  applyProfileValues(DEFAULT_PROFILE_NAME, DEFAULT_OBSTACLE_CM,
                     DEFAULT_HEIGHT_CODE, DEFAULT_VIB_LEVEL);
}

int findProfileIndex(const uint8_t *uid, uint8_t len)
{
  for (int i = 0; i < profileCount; i++)
  {
    if (profiles[i].uidLen == len && memcmp(profiles[i].uid, uid, len) == 0) return i;
  }
  return -1;
}

bool isRecordValid(const ProfileRecord &r)
{
  if (r.uidLen != 4 && r.uidLen != 7 && r.uidLen != 10) return false;
  if (r.heightCode < HEIGHT_CODE_MIN || r.heightCode > HEIGHT_CODE_MAX) return false;
  if (r.obstacleCm < OBSTACLE_CM_MIN || r.obstacleCm > OBSTACLE_CM_MAX) return false;
  if (r.vib > VIB_LEVEL_MAX) return false;
  size_t n = strnlen(r.name, sizeof(r.name));
  if (n == 0 || n > PROFILE_NAME_MAX_LEN) return false;
  for (size_t i = 0; i < n; ++i) if (!isalnum((unsigned char)r.name[i])) return false;
  return true;
}

// CRC-protected snapshots. Keep the previous file intact until atomic rename.
struct StoreHeader { uint32_t magic; uint16_t version; uint16_t count; uint32_t crc; };
constexpr uint32_t STORE_MAGIC = 0x4E415643;
constexpr char TEMP_PATH[] = "/profiles.tmp";
uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFF;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (int b = 0; b < 8; ++b) c = (c >> 1) ^ ((0u-(c&1u)) & 0xEDB88320u);
  }
  return ~c;
}
bool mountProfiles() {
  if (LittleFS.begin(false)) return true;
  // Format only a genuinely blank factory partition, never a corrupt existing store.
  const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
  if (!part) return false;
  uint8_t block[256];
  for (size_t pos = 0; pos < part->size; pos += sizeof(block)) {
    if (esp_partition_read(part, pos, block, sizeof(block)) != ESP_OK) return false;
    for (uint8_t byte : block) if (byte != 0xFF) return false;
  }
  if (!LittleFS.format()) return false;
  return LittleFS.begin(false);
}
void loadProfiles() {
  profileCount = 0; profileStoreReady = mountProfiles();
  if (!profileStoreReady) { Serial.println("[카드 저장소 오류] 파일시스템을 열지 못했습니다. 기존 데이터는 보존하며 카드 등록을 중지합니다."); return; }
  if (!LittleFS.exists(PROFILE_FILE_PATH)) { Serial.println("[카드 저장소] 아직 등록된 카드 파일이 없습니다. 최초 등록 전에는 정상입니다."); return; }
  File f = LittleFS.open(PROFILE_FILE_PATH, FILE_READ);
  if (!f) { profileStoreReady = false; return; }
  StoreHeader h = {}; bool header = f.read((uint8_t *)&h, sizeof(h)) == sizeof(h) && h.magic == STORE_MAGIC;
  size_t size = f.size();
  int count = header ? h.count : size / sizeof(ProfileRecord);
  bool valid = count >= 0 && count <= PROFILE_MAX_COUNT;
  if (header) valid = valid && h.version == 1 && size == sizeof(h)+count*sizeof(ProfileRecord);
  else { valid = valid && size % sizeof(ProfileRecord) == 0; f.seek(0); }
  if (valid && count) valid = f.read((uint8_t *)profiles, count*sizeof(ProfileRecord)) == count*sizeof(ProfileRecord);
  f.close();
  if (valid && header) valid = crc32((uint8_t *)profiles, count*sizeof(ProfileRecord)) == h.crc;
  for (int i = 0; valid && i < count; ++i) valid = isRecordValid(profiles[i]);
  if (!valid) {
    profileStoreReady = false; // do not overwrite a damaged file on next registration
    Serial.println("[카드 저장소 오류] 프로필 파일이 손상됐습니다. 파일은 보존하며 카드 등록을 중지합니다.");
  } else { profileCount = count; Serial.printf("[카드 저장소] %d명 불러오기 완료 (이전 파일 형식=%d)\n", count, !header); }
}
bool saveProfileAt(int index, const ProfileRecord &rec, bool isNew) {
  if (!profileStoreReady) return false;
  const int count = profileCount + (isNew ? 1 : 0);
  // Build the prospective snapshot in RAM, restore it until durable commit succeeds.
  const ProfileRecord previous = profiles[index];
  profiles[index] = rec;
  StoreHeader h = {STORE_MAGIC, 1, uint16_t(count), crc32((uint8_t *)profiles, count*sizeof(ProfileRecord))};
  File f = LittleFS.open(TEMP_PATH, FILE_WRITE);
  bool ok = bool(f);
  if (ok) ok = f.write((uint8_t *)&h, sizeof(h)) == sizeof(h);
  if (ok) ok = f.write((uint8_t *)profiles, count*sizeof(ProfileRecord)) == count*sizeof(ProfileRecord);
  if (f) { f.flush(); f.close(); }
  profiles[index] = previous;
  if (!ok) return false;
  // Verify bytes before replacing the previous snapshot.
  f = LittleFS.open(TEMP_PATH, FILE_READ);
  StoreHeader check = {};
  ok = bool(f) && f.size() == sizeof(h)+count*sizeof(ProfileRecord) &&
       f.read((uint8_t *)&check, sizeof(check)) == sizeof(check) && memcmp(&check, &h, sizeof(h)) == 0;
  uint32_t crc = 0xFFFFFFFF;
  while (ok && f.available()) {
    crc ^= uint8_t(f.read());
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ ((0u-(crc&1u)) & 0xEDB88320u);
  }
  if (f) f.close();
  return ok && ~crc == h.crc && LittleFS.rename(TEMP_PATH, PROFILE_FILE_PATH);
}

// ---------- BLE: CFG / REG ----------

bool parseStrictUInt(const String &s, long &out)
{
  if (s.length() == 0 || s.length() > 5) return false;
  for (unsigned int i = 0; i < s.length(); i++)
    if (s[i] < '0' || s[i] > '9') return false;
  out = s.toInt();
  return true;
}

// 반환: 0=OK, 1=FORMAT, 2=RANGE
int parseCfg(const String &line, PendingConfig &out)
{
  if (!line.startsWith("CFG,")) return 1;
  String fields[4];
  int start = 4;
  for (int i = 0; i < 4; i++)
  {
    int comma = line.indexOf(',', start);
    if (i < 3)
    {
      if (comma < 0) return 1;
      fields[i] = line.substring(start, comma);
      start = comma + 1;
    }
    else
    {
      if (comma >= 0) return 1;  // 필드 초과
      fields[i] = line.substring(start);
    }
  }

  const String &name = fields[0];
  if (name.length() < 1 || name.length() > (unsigned int)PROFILE_NAME_MAX_LEN) return 1;
  for (unsigned int i = 0; i < name.length(); i++)
    if (!isalnum((unsigned char)name[i])) return 1;

  long obstacle, height, vib;
  if (!parseStrictUInt(fields[1], obstacle) ||
      !parseStrictUInt(fields[2], height) ||
      !parseStrictUInt(fields[3], vib)) return 1;

  if (obstacle < OBSTACLE_CM_MIN || obstacle > OBSTACLE_CM_MAX) return 2;
  if (height < HEIGHT_CODE_MIN || height > HEIGHT_CODE_MAX) return 2;
  if (vib < 0 || vib > VIB_LEVEL_MAX) return 2;

  memset(&out, 0, sizeof(out));
  strncpy(out.name, name.c_str(), PROFILE_NAME_MAX_LEN);
  out.obstacleCm = (uint16_t)obstacle;
  out.heightCode = (uint8_t)height;
  out.vib = (uint8_t)vib;
  out.valid = true;
  return 0;
}

void handleCfgCommand(const String &line)
{
  PendingConfig cfg;
  int result = parseCfg(line, cfg);
  if (result == 1) { sendProtocolMessage("ERR,CFG,FORMAT"); return; }
  if (result == 2) { sendProtocolMessage("ERR,CFG,RANGE"); return; }
  pendingCfg = cfg;
  pendingCfg.session = bleSessionEpoch();
  profilesCancelRegistration("CONFIG_CHANGED"); // require a new REG for changed settings
  Serial.printf("[RFID] 대기 설정 수신: %s\n", pendingCfg.name);
  sendProtocolMessage("ACK,CFG");
}

void startRegistration()
{
  if (pendingCfg.valid && (!bleConnected() || pendingCfg.session != bleSessionEpoch())) profilesClearPending();
  if (!pendingCfg.valid) { sendProtocolMessage("REG,NOCFG"); return; }
  if (!profileStoreReady) { sendProtocolMessage("REG,FSERR"); return; }
  regWaiting = true;               // 이미 대기 중이면 15초 재시작
  regStartTime = millis();
  // Existing card may be registered on the next UID poll.
  Serial.println("[RFID] 등록 대기 시작 (15초)");
  sendProtocolMessage("REG,WAIT," + String(REG_WAIT_MS / 1000));
}

void updateRegistrationTimeout()
{
  if (!regWaiting) return;
  if (millis() - regStartTime < REG_WAIT_MS) return;
  regWaiting = false;              // 대기 설정은 유지
  Serial.println("[RFID] 등록 대기 시간 초과");
  sendProtocolMessage("REG,TIMEOUT");
  feedbackStart(Feedback::Failed);
}

void registerCard(const uint8_t *uid, uint8_t len, const String &uidHex)
{
  regWaiting = false;
  int idx = findProfileIndex(uid, len);
  bool isNew = idx < 0;
  if (isNew && profileCount >= PROFILE_MAX_COUNT)
  {
    sendProtocolMessage("REG,FULL");
    feedbackStart(Feedback::Failed);
    return;
  }

  ProfileRecord rec;
  memset(&rec, 0, sizeof(rec));
  memcpy(rec.uid, uid, len);
  rec.uidLen = len;
  strncpy(rec.name, pendingCfg.name, PROFILE_NAME_MAX_LEN);
  rec.obstacleCm = pendingCfg.obstacleCm;
  rec.heightCode = pendingCfg.heightCode;
  rec.vib = pendingCfg.vib;

  int target = isNew ? profileCount : idx;
  if (!saveProfileAt(target, rec, isNew))
  {
    Serial.println("[RFID] 프로필 저장 실패");
    sendProtocolMessage("REG,FSERR");
    feedbackStart(Feedback::Failed);
    return;
  }
  profiles[target] = rec;          // 저장 성공 후에만 RAM 반영
  if (isNew) profileCount++;

  applyProfileValues(rec.name, rec.obstacleCm, rec.heightCode, rec.vib);
  Serial.printf("[RFID] 등록 %s: %s (%s), 총 %d명\n",
                isNew ? "신규" : "덮어쓰기", uidHex.c_str(), rec.name, profileCount);
  sendProtocolMessage("REG,OK," + uidHex + (isNew ? ",NEW," : ",UPDATE,") + String(rec.name));
  feedbackStart(Feedback::Registered);
}

void handleCardDetected(const uint8_t *uid, uint8_t len)
{
  String uidHex = uidToHex(uid, len);
  unsigned long now = millis();

  if (regWaiting)
  {
    registerCard(uid, len, uidHex);
  }
  else
  {
    // 카드가 경계에서 흔들려 떨어짐/재인식이 반복돼도 같은 카드는 쿨다운 동안 무시
    bool sameCard = lastHandledUidLen == len && memcmp(lastHandledUid, uid, len) == 0;
    if (sameCard && now - lastHandledTime < RFID_SAME_CARD_COOLDOWN_MS) return;

    int idx = findProfileIndex(uid, len);
    if (idx >= 0)
    {
      const ProfileRecord &r = profiles[idx];
      applyProfileValues(r.name, r.obstacleCm, r.heightCode, r.vib);
      sendProtocolMessage("APPLY," + uidHex + "," + String(r.name));
      feedbackStart(Feedback::Applied);
    }
    else
    {
      Serial.println("[RFID] 미등록 카드 " + uidHex + " -> 기본 모드");
      applyDefaultProfile();
      sendProtocolMessage("UNREG," + uidHex);
      feedbackStart(Feedback::Unregistered);
    }
  }

  memcpy(lastHandledUid, uid, len);
  lastHandledUidLen = len;
  lastHandledTime = now;
}


void profilesBegin() { applyDefaultProfile(); loadProfiles(); }
void profilesCard(const uint8_t *uid, uint8_t length) {
  // Card polling happens independently of command processing. Validate before writing.
  if (regWaiting && (!bleConnected() || pendingCfg.session != bleSessionEpoch())) {
    profilesClearPending(); return;
  }
  if (regWaiting && uint32_t(millis()-regStartTime) >= REG_WAIT_MS) {
    updateRegistrationTimeout(); return;
  }
  if (!profileStoreReady) { sendProtocolMessage("REG,FSERR"); return; }
  if (length == 4 || length == 7 || length == 10) handleCardDetected(uid, length);
}
void profilesTick() {
  if (pendingCfg.valid && (!bleConnected() || pendingCfg.session != bleSessionEpoch())) profilesClearPending();
  updateRegistrationTimeout();
}
bool profilesStoreReady() { return profileStoreReady; }
void profilesClearPending() { pendingCfg.valid=false; profilesCancelRegistration("DISCONNECTED"); }
void profilesSerialCommand(const String &c) {
  if (c != "FS_INIT_CONFIRM_ERASE_PROFILES") return;
  // Maintenance-only explicit command; never run automatically or expose via BLE.
  if (profileStoreReady) { Serial.println("[카드 저장소] 정상 상태이므로 초기화를 거부합니다."); return; }
  LittleFS.end();
  if (!LittleFS.format()) { Serial.println("[카드 저장소 오류] 초기화에 실패했습니다."); return; }
  loadProfiles();
  Serial.println(profileStoreReady ? "[카드 저장소] 초기화 완료. 카드 등록을 사용할 수 있습니다." : "[카드 저장소 오류] 초기화 후에도 저장소를 사용할 수 없습니다.");
}
bool profilesWaiting() { return regWaiting; }
void profilesCancelRegistration(const char *reason) {
  if (regWaiting) { regWaiting = false; sendProtocolMessage(String("REG,CANCEL,")+reason); }
}
bool profilesCommand(const String &c) {
  if (c == "CFG" || c.startsWith("CFG,")) { handleCfgCommand(c); return true; }
  if (c == "REG") {
    if (!expansionRfidReady()) sendProtocolMessage("REG,NO_RFID");
    else startRegistration();
    return true;
  }
  if (c == "GET_PROFILE_STORE") { sendMessage(String("SENSOR:PROFILE_STORE:")+(profileStoreReady ? "READY" : "FAULT")); return true; }
  if (c == "REG_CANCEL") { profilesCancelRegistration("USER"); return true; }
  if (c.startsWith("SET_STAIR_CM,")) {
    long cm;
    if (!parseStrictUInt(c.substring(13), cm) || cm < 30 || cm > 600) sendMessage("ERR,STAIR,RANGE");
    else { activeStairThresholdCM = cm; sendMessage("ACK,STAIR,"+String(cm)); }
    return true;
  }
  return false;
}
