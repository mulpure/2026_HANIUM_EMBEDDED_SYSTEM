#pragma once
#include <Arduino.h>

// Serial descriptions only. Never use these strings as BLE protocol messages.
namespace nav {
inline String eventField(const String &line, unsigned wanted) {
  int start = 0;
  for (unsigned i = 0; i < wanted; ++i) {
    int comma = line.indexOf(',', start);
    if (comma < 0) return "";
    start = comma + 1;
  }
  int end = line.indexOf(',', start);
  return end < 0 ? line.substring(start) : line.substring(start, end);
}
inline String moduleKorean(const String &name) {
  if (name == "RFID") return "RFID 카드 모듈";
  if (name == "REMOTE") return "리모컨·초음파 모듈";
  if (name == "LED") return "LED 표시 모듈";
  if (name == "NONE") return "모듈 없음";
  return "종류를 판별할 수 없는 모듈";
}
inline String cancelKorean(const String &reason) {
  if (reason == "USER") return "사용자 요청";
  if (reason == "DETACHED") return "확장모듈 연결 끊김";
  if (reason == "RFID_FAULT") return "RFID 통신 이상";
  if (reason == "DISCONNECTED") return "휴대폰 연결 끊김";
  if (reason == "CONFIG_CHANGED") return "대기 설정 변경";
  return "등록 조건 변경";
}
inline String serialEventText(const String &s) {
  if (s == "FALL_DETECTED") return "[낙상] 낙상으로 판정했습니다. 중앙 진동과 휴대폰 알림을 요청합니다.";
  if (s == "계단 감지" || s == "STAIR_DETECTED") return "[계단] 위험 감지 상태입니다. 중앙 진동과 브레이크를 작동합니다.";
  if (s == "STAIR_CLEARED") return "[계단] 위험 해제 상태입니다.";
  if (s == "포고핀 재결합") return "[포고핀] 분절 센서 통신이 복구됐습니다. 실제 접촉을 직접 측정한 결과는 아닙니다.";
  if (s == "포고핀 분리상태") return "[포고핀] 분절 센서 응답이 끊겼습니다. 접점·전원·통신을 확인하세요.";
  if (s == "포고핀 연결확인 중") return "[포고핀] 센서 응답을 확인했습니다. 연결 안정 확인·초기화 중입니다.";
  if (s == "OBSTACLE:LEFT") return "[장애물] 왼쪽 장애물 감지";
  if (s == "OBSTACLE:RIGHT") return "[장애물] 오른쪽 장애물 감지";
  if (s == "OBSTACLE:BOTH") return "[장애물] 양쪽 장애물 감지";
  if (s == "OBSTACLE:NONE") return "[장애물] 좌우 장애물 경고 해제 상태";
  if (s == "OBSTACLE:UPPER") return "[장애물] 상부 장애물 감지";
  if (s == "OBSTACLE:UPPER_CLEAR") return "[장애물] 상부 장애물 경고 해제 상태";
  if (s == "BUTTON_PRESSED") return "[버튼] 상단 버튼 입력. Pi와 휴대폰에 전달합니다. GPIO33은 버튼, 중앙 진동은 GPIO32입니다.";
  if (s == "GREEN_LIGHT") return "[신호등] Pi에서 초록불 감지 신호를 받았습니다.";
  if (s == "YOLO:ON") return "[영상인식] YOLO 활성 상태";
  if (s == "YOLO:OFF") return "[영상인식] YOLO 비활성 상태";
  if (s == "CONNECTED") return "[휴대폰] BLE 알림 준비 완료. 현재 상태를 동기화합니다.";
  if (s == "PONG") return "[통신] 통신 확인 요청에 응답했습니다.";
  if (s == "ACK,STREAM") return "[통신] 줄바꿈 메시지 조립 모드를 사용합니다.";
  if (s == "ACK,CFG") return "[설정] 카드 등록용 대기 설정 저장 완료. 현재 사용자 설정은 아직 바뀌지 않았습니다.";
  if (s.startsWith("ACK,STAIR,")) return "[보정] 임시 계단 기준 거리: " + eventField(s,2) + "cm";
  if (s.startsWith("REG,WAIT,")) return "[카드] 등록 대기 중. " + eventField(s,2) + "초 안에 카드를 태그하세요.";
  if (s.startsWith("REG,OK,")) return "[카드] 등록 성공: " + eventField(s,4) + (eventField(s,3)=="UPDATE" ? " (기존 카드 갱신)" : " (신규 카드)");
  if (s == "REG,NOCFG") return "[카드 오류] 대기 설정이 없습니다. 앱에서 설정을 먼저 전송하세요.";
  if (s == "REG,NO_RFID") return "[카드 오류] RFID 모듈이 준비되지 않았습니다. 슬롯 배선과 모듈을 확인하세요.";
  if (s == "REG,FSERR") return "[카드 오류] 저장소 이상으로 등록할 수 없습니다. 기존 데이터를 자동 삭제하지 않습니다.";
  if (s == "REG,FULL") return "[카드 오류] 등록 한도 500명에 도달했습니다.";
  if (s == "REG,TIMEOUT") return "[카드] 등록 대기 시간이 끝났습니다. 다시 등록을 시작하세요.";
  if (s.startsWith("REG,CANCEL,")) return "[카드] 등록 취소: " + cancelKorean(eventField(s,2));
  if (s.startsWith("APPLY,")) return "[카드] 저장된 사용자 설정 적용: " + eventField(s,2);
  if (s.startsWith("UNREG,")) return "[카드] 미등록 카드입니다. 기본 사용자 설정을 적용합니다.";
  if (s.startsWith("ERR,CFG,")) return "[설정 오류] 형식 또는 범위를 확인하세요. 이름 영문·숫자 1~12자, 거리 20~200cm, 키 코드 14~19, 진동 0/1.";
  if (s == "ERR,STAIR,RANGE") return "[보정 오류] 계단 기준 거리는 30~600cm입니다.";
  if (s.startsWith("ERR,QUEUE,")) return "[통신 오류] 명령 대기열이 가득 찼습니다. 잠시 후 다시 요청하세요.";
  if (s.startsWith("ERR,COMMAND,")) return "[통신 오류] 명령이 너무 길거나 형식이 잘못됐습니다. 최대 96바이트입니다.";
  if (s == "ERROR:MTU_LOW") return "[통신 경고] 메시지 전송 크기가 작습니다. 앱의 MTU 협상 또는 줄바꿈 조립 모드를 확인하세요.";
  if (s == "ERROR:UNKNOWN_COMMAND") return "[명령 오류] 지원하지 않는 명령입니다.";
  if (s == "ERROR:ULTRASONIC_STUCK_HIGH") return "[초음파 오류] ECHO가 HIGH에 머물렀습니다. ECHO 배선·전압·센서 또는 슬롯 오인식을 확인하세요.";
  if (s == "ERROR:ULTRASONIC_NO_RESPONSE") return "[초음파 오류] 비정상 응답이 반복됩니다. TRIG·ECHO·전원·슬롯 ID를 확인하세요.";
  if (s == "MODULE,UNKNOWN,ID_INVALID") return "[슬롯 오류] ID 전압으로 모듈 종류를 판별할 수 없습니다. 슬롯 미구성이라면 GPIO36의 외부 10kΩ 풀업이 필요합니다. 고장 경고 진동은 사용하지 않습니다.";
  if (s.startsWith("MODULE,")) {
    String state = eventField(s,2);
    String description = state=="READY" ? "사용 준비 완료" : state=="INITIALIZING" ? "초기화 중" :
                         state=="FAULT" ? "통신 이상. 분리·재장착 또는 배선을 확인하세요." : "연결 상태";
    return "[슬롯] " + moduleKorean(eventField(s,1)) + ": " + description;
  }
  if (s == "REMOTE,TRIGGERED") return "[리모컨] 버튼 동작 신호를 출력했습니다.";
  if (s == "LED,TEST") return "[LED] 표시등 순차 점등 시험 시작";
  if (s == "ERR,LED,NO_MODULE") return "[LED 오류] LED 슬롯 모듈이 연결되지 않았습니다.";
  if (s == "LED,AUTO_HEALTH") return "[LED] 센서 정상·BLE·YOLO 상태를 자동 표시합니다.";
  if (s == "LED:MODULE") return "[LED] LED 모듈 연결 상태";
  if (s == "LED:ABSENT") return "[LED] LED 모듈 없음";
  if (s.startsWith("SENSOR:")) {
    int end = s.indexOf(':',7);
    if (end >= 0) {
      String name=s.substring(7,end), state=s.substring(end+1);
      String label=name=="MPU" ? "가속도 MPU6050" : name=="STAIR" ? "계단 VL53L4CX" :
                   name=="OBSTACLE" ? "장애물 VL53L5CX" : name=="PROFILE_STORE" ? "카드 저장소" :
                   name=="ULTRASONIC" ? "상부 초음파" : name=="STAIR_ALERT" ? "계단 경고" : name;
      String detail=state=="READY" ? "사용 가능" : state=="FAULT" ? "사용 불가. 미연결·통신 실패·측정 갱신 지연 여부는 상태/원인 로그를 확인하세요." :
                    state=="ABSENT" ? "모듈 없음" : state=="FAR" ? "측정 범위 밖" :
                    state=="DISTANCE" ? "거리 측정 중" : state=="NO_RESPONSE" ? "응답 없음" :
                    state=="STUCK_HIGH" ? "ECHO가 HIGH에 고정됨" : state=="INVALID" ? "비정상 펄스" :
                    state=="1" ? "위험 감지 상태" : state=="0" ? "위험 해제 상태" : state;
      return "[센서] " + label + ": " + detail;
    }
    return "[센서] 사용하지 않는 입력입니다.";
  }
  if (s.startsWith("ID_MV:")) return "[슬롯] ID 전압: " + s.substring(6) + "mV";
  if (s.startsWith("ACCEL:")) return "[가속도] " + s.substring(6) + "m/s² (-1은 사용 불가)";
  if (s.startsWith("DISTANCE:")) return "[장애물 거리] " + s.substring(9) + "cm (-1은 유효 거리 없음)";
  if (s.startsWith("STAIR_DISTANCE:")) return "[계단 거리] " + s.substring(15) + "cm (-1은 사용 불가)";
  if (s.startsWith("ULTRASONIC:")) return "[초음파 거리] " + s.substring(11) + "cm (-1은 유효 거리 없음)";
  if (s.startsWith("MPU_MAX_GAP_MS:")) return "[가속도] 최대 측정 갱신 간격: " + s.substring(15) + "ms";
  if (s.startsWith("PROFILE,")) return "[사용자] " + eventField(s,1) + " / 장애물 " + eventField(s,2) + "cm / 키 코드 " + eventField(s,3) + " / 진동 " + eventField(s,4);
  if (s == "SYSTEM,SENSOR_TASK_FAILED") return "[시스템 오류] 센서 처리 태스크를 만들지 못했습니다. 메모리와 부팅 상태를 확인하세요.";
  return "[기기 알림] " + s;
}
}
