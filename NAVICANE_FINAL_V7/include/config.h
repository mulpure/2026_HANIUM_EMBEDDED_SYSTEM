#pragma once
#include <stdint.h>
namespace cfg {
constexpr bool SERIAL_RAW_PROTOCOL = false; // true also prints the original BLE event on Serial
constexpr bool MODULE_LOG_ONLY = false;     // 전체 센서 상태를 1초마다 한 줄로 출력
constexpr char DEVICE_NAME[] = "NAVCANE_ESP32";
constexpr char SERVICE_UUID[] = "12345678-1234-1234-1234-1234567890ab";
constexpr char CHARACTERISTIC_UUID[] = "abcd1234-1234-1234-1234-abcdef123456";
constexpr uint8_t OBSTACLE_SDA = 18, OBSTACLE_SCL = 19, OBSTACLE_LPN = 25;
constexpr uint8_t SEGMENT_SDA = 21, SEGMENT_SCL = 22;
constexpr uint8_t VIB_MAIN = 32, VIB_LEFT = 14, VIB_RIGHT = 26;
constexpr uint8_t VIB_CH_MAIN = 0, VIB_CH_LEFT = 2, VIB_CH_RIGHT = 4;
constexpr uint8_t SERVO = 23, SERVO_CHANNEL = 7, BUTTON = 33;
constexpr uint8_t UART_RX = 16, UART_TX = 17;
constexpr uint8_t SLOT_ID = 36, S1 = 4, S2 = 13, S3 = 35, S4 = 5;
constexpr uint32_t ID_SETTLE_MS = 300, ID_POLL_MS = 5;
constexpr uint32_t ID_LEAVE_MS = 100, ID_RESUME_MS = 30;
constexpr uint32_t POGO_SETTLE_MS = 3000, POGO_PROBE_MS = 200;
constexpr uint8_t POGO_STABLE_COUNT = 5;
constexpr uint32_t MPU_PERIOD_MS = 15;
// Expected divider voltages: 595 / 1650 / 2533 mV. Calibrate if needed.
constexpr uint16_t RFID_ID_MIN_MV = 380, RFID_ID_MAX_MV = 850;
constexpr uint16_t REMOTE_ID_MIN_MV = 1250, REMOTE_ID_MAX_MV = 2050;
constexpr uint16_t LED_ID_MIN_MV = 2250, LED_ID_MAX_MV = 2850;
constexpr uint32_t I2C_TIMEOUT_MS = 25, SENSOR_RETRY_MS = 5000;
// Separate buses: preserve the original L5CX timeout without delaying shared MPU I2C.
constexpr uint32_t OBSTACLE_I2C_TIMEOUT_MS = 500;
constexpr uint32_t STAIR_STALE_MS = 600, OBSTACLE_STALE_MS = 600, ACCEL_STALE_MS = 120;
constexpr uint32_t OBSTACLE_REINIT_MS = 2500;
// VL53L5CX: 이전 단일 파일(정상 동작) 버전과 같은 복구 주기
constexpr uint32_t OBSTACLE_RETRY_MS = 3000;   // 초기화 실패 후 재시도 간격
constexpr uint32_t OBSTACLE_ACK_MS = 500;      // 0x29 응답 확인 주기
constexpr uint8_t OBSTACLE_ACK_MISS_LIMIT = 2; // 연속 미응답 시 재초기화
constexpr uint32_t OBSTACLE_INIT_CLOCK_HZ = 100000, OBSTACLE_RUN_CLOCK_HZ = 400000;
// At age >= 5 min the pending BLE event expires, even while disconnected.
constexpr uint32_t FALL_PENDING_MAX_MS = 5u * 60u * 1000u;
constexpr uint32_t SENSOR_HEALTH_MS = 500;
constexpr uint8_t SENSOR_HEALTH_MISS_LIMIT = 2;
constexpr uint32_t OBSTACLE_LPN_LOW_MS = 200, OBSTACLE_BOOT_MS = 500;
constexpr uint8_t OBSTACLE_ROW_MIN = 0, OBSTACLE_ROW_MAX = 2;
constexpr bool SWAP_OBSTACLE_SIDES = false;
constexpr uint16_t VIB_WEAK_DUTY = 150, VIB_STRONG_DUTY = 255;
constexpr int SERVO_RELEASE_DEG = 0, SERVO_APPLY_DEG = 90;
constexpr uint16_t SERVO_MIN_US = 500, SERVO_MAX_US = 2400;
constexpr uint16_t DEFAULT_OBSTACLE_CM = 50;
constexpr float DEFAULT_STAIR_CM = 170.0f;
// Calibrate on the assembled cane. Height alone is NOT the floor slant distance.
constexpr float STAIR_CM_BY_HEIGHT[6] = {170, 170, 170, 170, 170, 170};
constexpr uint8_t DEFAULT_HEIGHT_CODE = 17, DEFAULT_VIB_LEVEL = 1;
constexpr uint32_t REG_WAIT_MS = 15000;
constexpr bool STAIR_NO_TARGET_ALERT = true;
constexpr uint32_t REMOTE_PULSE_MS = 200;
constexpr uint32_t ULTRASONIC_PERIOD_MS = 100, ULTRASONIC_TIMEOUT_US = 60000;
constexpr uint8_t ULTRASONIC_MISS_LIMIT = 10;
constexpr float ULTRASONIC_MIN_CM = 2, ULTRASONIC_MAX_CM = 400;
constexpr float UPPER_OBSTACLE_CM = 120, UPPER_RELEASE_CM = 130;
}
