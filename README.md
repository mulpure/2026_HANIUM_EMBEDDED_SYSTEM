# 2026_HANIUM_EMBEDDED_SYSTEM
한이음드림업 임베디드 시스템 파트<br>
코드 작성 및 관리 : 영남대학교 정보통신공 김동하

# NAVCANE Change Log

## 2026-08

### Hardware
- 계단 감지 센서를 초음파 센서에서 VL53L1X로 변경
- 좌우 장애물 감지를 위해 VL53L5CX 적용
- 낙상 감지를 위해 MPU6050 적용

### Circuit
- ESP32 I2C를 SDA GPIO21 / SCL GPIO22로 통합
- VL53L1X 주소를 0x29 → 0x30으로 변경
- VL53L5CX 주소는 0x29 유지
- VL53L5CX LPn을 이용해 센서 초기화 순서 제어

### Raspberry Pi
- Raspberry Pi 5는 횡단보도 관련 기능만 담당하도록 역할 분리
- Camera Module 3 + YOLO 기반 보행자 신호등 인식 적용
- ONNX 모델 사용
- navcane-yolo.service를 이용한 실행 관리 적용

### ESP32
- 낙상 감지
- 계단 감지
- 좌우 장애물 감지
- 진동 모터 제어
- Bluetooth 통신 담당

### Communication
- Raspberry Pi ↔ ESP32 UART 통신 적용
- ESP32 버튼 입력으로 Raspberry Pi의 YOLO 실행
- 녹색 신호 감지 결과를 Raspberry Pi에서 ESP32로 전송

### Behavior
- 버튼 입력 시 YOLO를 최대 5분간 실행
- 녹색 신호 감지 시 진동 패턴 실행
- 진동 패턴: 짧게 → 길게 → 짧게 → 길게
- 녹색 신호 감지 이후 YOLO 남은 실행시간을 약 1분으로 단축

### Power
- 일부 보조배터리에서 ESP32 저전류 자동 차단 현상 확인
- Raspberry Pi에서 카메라/YOLO 실행 순간 전원 차단 현상 확인
- 안정적인 5V 출력이 가능한 전원 제품으로 변경 검토
