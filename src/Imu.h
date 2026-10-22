#pragma once

// QMI8658 accelerometer, used only to notice that the board was picked up or
// moved while the screen is off. Register access over the shared I2C bus;
// the caller holds the bus lock.
//
// Detection: the board's orientation when the screen went off is the
// reference. A tilt of more than LIFT_TILT_DEG away from it, or a clear jolt
// (acceleration changing by more than LIFT_JOLT_G between samples on
// consecutive reads), counts as a lift.

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "Config.h"

class Imu {
public:
  bool begin() {
    for (uint8_t a : {uint8_t(0x6B), uint8_t(0x6A)}) {
      uint8_t id = 0;
      if (read(a, 0x00, &id, 1) && id == 0x05) {
        addr_ = a;
        break;
      }
    }
    if (!addr_) {
      Serial.println("[IMU] QMI8658 not found");
      return false;
    }
    write(0x60, 0xB0); // soft reset
    delay(15);
    write(0x02, 0x40); // CTRL1: register address auto-increment
    write(0x03, 0x17); // CTRL2: accelerometer +-4 g, 62.5 Hz
    write(0x08, 0x01); // CTRL7: accelerometer on, gyroscope off
    delay(20);
    Serial.printf("[IMU] QMI8658 at 0x%02X\n", addr_);
    return true;
  }

  bool ready() const { return addr_ != 0; }

  // Acceleration in g.
  bool accel(float &x, float &y, float &z) {
    uint8_t b[6];
    if (!addr_ || !read(addr_, 0x35, b, 6)) return false;
    constexpr float kScale = 1.0f / 8192.0f; // +-4 g
    x = static_cast<int16_t>(b[0] | b[1] << 8) * kScale;
    y = static_cast<int16_t>(b[2] | b[3] << 8) * kScale;
    z = static_cast<int16_t>(b[4] | b[5] << 8) * kScale;
    return true;
  }

  // Remember the current orientation as "at rest".
  void arm() {
    float x, y, z;
    armed_ = accel(x, y, z);
    if (!armed_) return;
    rx_ = x; ry_ = y; rz_ = z;
    px_ = x; py_ = y; pz_ = z;
    jolts_ = 0;
  }

  void disarm() { armed_ = false; }

  // True once when the board was lifted or moved since arm().
  bool lifted() {
    if (!armed_) return false;
    float x, y, z;
    if (!accel(x, y, z)) return false;
    const float jolt = sqrtf(square(x - px_) + square(y - py_) + square(z - pz_));
    px_ = x; py_ = y; pz_ = z;
    jolts_ = jolt > LIFT_JOLT_G ? jolts_ + 1 : 0;

    const float n0 = sqrtf(rx_ * rx_ + ry_ * ry_ + rz_ * rz_);
    const float n1 = sqrtf(x * x + y * y + z * z);
    float tilt = 0;
    if (n0 > 0.3f && n1 > 0.3f) {
      const float c = constrain((rx_ * x + ry_ * y + rz_ * z) / (n0 * n1), -1.0f, 1.0f);
      tilt = acosf(c) * 57.2958f;
    }
    if (tilt > LIFT_TILT_DEG || jolts_ >= 2) {
      Serial.printf("[IMU] lift: tilt %.0f deg, jolt %.2f g\n", tilt, jolt);
      armed_ = false;
      return true;
    }
    return false;
  }

private:
  bool write(uint8_t reg, uint8_t v) {
    Wire.beginTransmission(addr_);
    Wire.write(reg);
    Wire.write(v);
    return Wire.endTransmission() == 0;
  }
  static bool read(uint8_t addr, uint8_t reg, uint8_t *out, size_t n) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, static_cast<uint8_t>(n)) != n) return false;
    for (size_t i = 0; i < n; i++) out[i] = Wire.read();
    return true;
  }
  static float square(float v) { return v * v; }

  uint8_t addr_ = 0;
  bool armed_ = false;
  float rx_ = 0, ry_ = 0, rz_ = 0; // orientation at rest
  float px_ = 0, py_ = 0, pz_ = 0; // previous sample
  int jolts_ = 0;
};
