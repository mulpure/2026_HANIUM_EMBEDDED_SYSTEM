package com.example.whitecaneguardian

// RFID 사용자 프로필 연동 상수 (ESP32 팀 확정값 반영)
object RfidConfig {
    const val REQUEST_MTU = 185

    // 이름: 영문/숫자 1~12자, 쉼표·공백 불가
    const val NAME_MAX_LEN = 12
    val NAME_REGEX = Regex("^[A-Za-z0-9]{1,$NAME_MAX_LEN}$")

    // 장애물 감지 시작 거리 (cm) — 확정: 20~200, 기본 50
    const val OBSTACLE_MIN_CM = 20
    const val OBSTACLE_MAX_CM = 200
    const val OBSTACLE_DEFAULT_CM = 50

    // 키 코드 (height_code = 키/10) — 확정: 14~19, 기본 17
    const val HEIGHT_CODE_MIN = 14
    const val HEIGHT_CODE_MAX = 19
    const val HEIGHT_CODE_DEFAULT = 17

    // 진동 세기 — 확정: 0(약)/1(강), 기본 1
    const val VIB_STRONG = 1
    const val VIB_WEAK = 0

    // 카드 등록 대기 시간 (초)
    const val REG_WAIT_SEC = 15

    // 응답 타임아웃 (ms)
    const val ACK_TIMEOUT_MS = 5000L
}