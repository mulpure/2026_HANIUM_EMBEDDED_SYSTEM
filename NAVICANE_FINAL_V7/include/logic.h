#pragma once
#include <stdint.h>
#include <math.h>
#include "config.h"
namespace nav {
enum class Module : uint8_t { None, Rfid, Remote, Led, Unknown };
inline Module identify(uint16_t mv) {
  if (mv >= cfg::RFID_ID_MIN_MV && mv <= cfg::RFID_ID_MAX_MV) return Module::Rfid;
  if (mv >= cfg::REMOTE_ID_MIN_MV && mv <= cfg::REMOTE_ID_MAX_MV) return Module::Remote;
  if (mv >= cfg::LED_ID_MIN_MV && mv <= cfg::LED_ID_MAX_MV) return Module::Led;
  return mv >= 2950 ? Module::None : Module::Unknown;
}
inline bool elapsed(uint32_t now, uint32_t then, uint32_t period) {
  return uint32_t(now - then) >= period;
}
// Keeping the driver alive lets a transient missing frame recover without firmware upload.
// Only a failed initialization uses backoff; a stalled running sensor restarts immediately.
class ObstacleRecovery {
 public:
  bool running() const { return ready; }
  bool shouldInitialize(uint32_t now) const {
    return !ready && (!attempted || elapsed(now, attemptAt, cfg::OBSTACLE_RETRY_MS));
  }
  void initialized(bool ok, uint32_t completedAt) {
    ready = ok; attempted = true; attemptAt = frameAt = completedAt;
  }
  void frame(uint32_t now) { frameAt = now; }
  // 0x29 연속 미응답: 즉시 재초기화 대상으로 전환
  void lost() { ready = false; attempted = false; }
  bool needsRestart(uint32_t now) {
    if (!ready || !elapsed(now, frameAt, cfg::OBSTACLE_REINIT_MS)) return false;
    ready = false; attempted = false; return true;
  }
 private:
  bool ready = false, attempted = false;
  uint32_t attemptAt = 0, frameAt = 0;
};
// Electrical permission and logical attachment are intentionally separate.
// First mismatch revokes outputs; only a sustained mismatch detaches the module.
class SlotGate {
 public:
  Module active = Module::None;
  bool permitted = false;
  bool update(Module observed, uint32_t now) {
    const Module old = active;
    if (observed != candidate) { candidate = observed; since = now; }
    if (active != Module::None) {
      if (observed != active) {
        permitted = false; returning = false;
        if (!leaving) { leaving = true; leaveAt = now; }
        if (elapsed(now, leaveAt, cfg::ID_LEAVE_MS)) { active = Module::None; leaving = false; }
      } else {
        leaving = false;
        if (!permitted) {
          if (!returning) { returning = true; returnAt = now; }
          if (elapsed(now, returnAt, cfg::ID_RESUME_MS)) { permitted = true; returning = false; }
        }
      }
    }
    if (active == Module::None && supported(candidate) && elapsed(now, since, cfg::ID_SETTLE_MS)) {
      active = candidate; permitted = true; leaving = returning = false;
    }
    return old != active;
  }
 private:
  static bool supported(Module m) { return m == Module::Rfid || m == Module::Remote || m == Module::Led; }
  Module candidate = Module::None;
  uint32_t since = 0, leaveAt = 0, returnAt = 0;
  bool leaving = false, returning = false;
};
// Wait for continuous electrical presence, then five independently spaced probes.
class ReconnectGate {
 public:
  bool update(bool present, uint32_t now) {
    if (!present) { reset(); return false; }
    if (!seen) { seen = true; since = now; last = now; }
    if (!elapsed(now, since, cfg::POGO_SETTLE_MS)) return false;
    if (count < cfg::POGO_STABLE_COUNT && elapsed(now, last, cfg::POGO_PROBE_MS)) {
      last = now; ++count;
    }
    return count >= cfg::POGO_STABLE_COUNT;
  }
  void reset() { seen = false; count = 0; }
 private:
  bool seen = false;
  uint8_t count = 0;
  uint32_t since = 0, last = 0;
};
enum class EchoResult { Distance, Far, NoResponse, StuckHigh, Invalid };
inline EchoResult classifyEcho(bool done, bool sawRise, bool high, uint32_t width) {
  if (!done) return high || sawRise ? EchoResult::StuckHigh : EchoResult::NoResponse;
  if (width < 60 || width > cfg::ULTRASONIC_TIMEOUT_US) return EchoResult::Invalid;
  float cm = width * 0.0343f / 2;
  return cm < cfg::ULTRASONIC_MIN_CM || cm > cfg::ULTRASONIC_MAX_CM ? EchoResult::Far : EchoResult::Distance;
}
class UpperDecision {
 public:
  bool alert = false;
  float warningCm = -1;
  void reset() { alert = false; warningCm = -1; above = below = 0; }
  // Missing samples break confirmation but cannot prove the obstacle disappeared.
  void invalid() { above = below = 0; }
  void sample(float cm) {
    if (!isfinite(cm)) { invalid(); return; }
    if (cm >= 0 && cm < cfg::UPPER_OBSTACLE_CM) {
      below = 0; if (above < 2) ++above; if (above >= 2) alert = true;
      if (alert) warningCm = cm;
    } else if (cm < 0 || cm > cfg::UPPER_RELEASE_CM) {
      above = 0; if (below < 2) ++below;
      if (below >= 2) { alert = false; warningCm = -1; }
    } else {
      above = below = 0;
      if (alert) warningCm = cm;
    }
  }
 private:
  uint8_t above = 0, below = 0;
};
inline uint8_t normalLeds(bool obstacle, bool mpu, bool stair, bool ble, bool yolo) {
  return (obstacle?1:0) | (mpu?2:0) | (stair?4:0) | (ble?8:0) | (yolo?16:0);
}
class Mean3 {
 public:
  void reset() { count = next = 0; }
  float sample(float value) {
    if (!isfinite(value) || value < 0) { reset(); return -1; }
    values[next] = value; next = (next + 1) % 3;
    if (count < 3) ++count;
    float sum = 0; for (uint8_t i=0; i<count; ++i) sum += values[i];
    return sum/count;
  }
 private:
  float values[3] = {};
  uint8_t count = 0, next = 0;
};
class StairDecision {
 public:
  bool alert = false;
  void reset() { alert = false; above = below = invalid = 0; }
  void sample(float cm, bool noTarget, float threshold) {
    const bool risk = (isfinite(cm) && cm >= 0 && cm > threshold)
                      || (noTarget && cfg::STAIR_NO_TARGET_ALERT);
    if (risk) {
      below = 0; invalid = 0;
      if (above < 2) ++above;
      if (above >= 2) alert = true;
    } else if (isfinite(cm) && cm >= 0) {
      above = 0; invalid = 0;
      if (cm < threshold - 5) {
        if (below < 3) ++below;
        if (below >= 3) alert = false;
      } else below = 0;
    } else {
      above = below = 0;
      if (invalid < 3) ++invalid;
      if (invalid >= 3) alert = true;
    }
  }
 private:
  uint8_t above = 0, below = 0, invalid = 0;
};
inline float accelMagnitude(const uint8_t *raw) {
  float sum = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    int16_t value = int16_t((uint16_t(raw[2*i]) << 8) | raw[2*i+1]);
    float acceleration = value*(9.80665f/4096.0f); // +/-8g
    sum += acceleration*acceleration;
  }
  return sqrtf(sum);
}
class FallDetector {
 public:
  void reset() { state = 0; }
  bool sample(float magnitude, uint32_t now) {
    if (!isfinite(magnitude)) { reset(); return false; }
    switch (state) {
      case 0: if (magnitude < 5.0f) { state = 1; since = now; } break;
      case 1:
        // Check the deadline before accepting an impact from a later movement.
        if (elapsed(now, since, 600)) reset();
        else if (magnitude > 10.0f) { state = 2; since = now; }
        break;
      case 2:
        if (!elapsed(now, since, 500)) break;
        if (magnitude < 7.0f || magnitude > 12.5f) reset();
        else if (elapsed(now, since, 1500)) { reset(); return true; }
        break;
    }
    return false;
  }
 private:
  uint8_t state = 0;
  uint32_t since = 0;
};
}
