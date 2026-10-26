# Android 교체 파일

기존 com.example.whitecaneguardian 패키지의 세 파일을 이 폴더의 파일로 함께 교체하세요.

- BleManager.kt: 주 수신자 유지 + 추가 수신자, MTU/CCCD/쓰기 오류, UTF-8 줄바꿈 프레임, STREAM_ON.
- RfidFragment.kt: 대기 CFG 흐름, ACK,CFG만 인정, 오류·취소·타임아웃, 화면 종료 시 수신 해제.
- RfidConfig.kt: 첨부 앱의 거리·키 코드·진동·등록 시간 상수 유지.

compileSdk 33 이상이 필요합니다. Manifest 권한 요청, BleHolder/BleLog/VoiceTrigger, Firebase 및 기존 화면 XML은 기존 프로젝트에서 사용합니다. 파일을 추가해 같은 이름의 클래스를 중복 선언하지 마세요.

RFID 화면 외에도 BLE listener를 바꾸는 화면이 있다면 addListener/removeListener 방식으로 연결하세요. APK 빌드와 실물 휴대폰 통합 시험은 기존 전체 프로젝트에서 수행해야 합니다.
