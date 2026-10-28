#pragma once

// Touch input for the Waveshare ESP32-S3-Touch-AMOLED-1.8.
//
// The board has shipped with an FT3168 (I2C 0x38) and, on V2 boards, a
// CST820 (I2C 0x15). Both sit on the shared I2C bus; whichever answers is
// used. Their reset lines go through the TCA9554 I/O expander (0x20, pins
// 0-2), which is pulsed once at boot before the panel is initialized, as in
// Waveshare's own examples.

#include <Arduino.h>
#include <Wire.h>

#include "Config.h"

class TouchInput {
public:
  enum class Chip { None, FT3168, CST820 };

  // Pulse the expander's reset outputs. Call before the panel's begin().
  static void resetViaExpander() {
    constexpr uint8_t kExpander = 0x20;
    constexpr uint8_t kRegOutput = 0x01, kRegConfig = 0x03;
    // Pins 0-2 as outputs (0 = output), the rest stay inputs.
    writeReg(kExpander, kRegOutput, 0xF8);
    if (!writeReg(kExpander, kRegConfig, 0xF8)) {
      Serial.println("[Touch] TCA9554 expander not found");
      return;
    }
    delay(20);
    writeReg(kExpander, kRegOutput, 0xFF);
    delay(120);
  }

  bool begin() {
    pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
    probe();
    return _chip != Chip::None;
  }

  Chip chip() const { return _chip; }
  const char *chipName() const {
    return _chip == Chip::FT3168 ? "FT3168" : _chip == Chip::CST820 ? "CST820" : "none";
  }

  // Read the first touch point. Returns true while a finger is down.
  bool read(int &x, int &y) {
    if (_chip == Chip::None) {
      // The CST820 can be asleep at boot; look again now and then.
      if (millis() - _lastProbeMs > 2000) probe();
      return false;
    }
    uint8_t b[6];
    int rawX = 0, rawY = 0;
    if (_chip == Chip::FT3168) {
      // 0x02 finger count, 0x03..0x06 X high/low, Y high/low.
      if (!readRegs(kFtAddr, 0x02, b, 5) || (b[0] & 0x0F) == 0) return false;
      rawX = ((b[1] & 0x0F) << 8) | b[2];
      rawY = ((b[3] & 0x0F) << 8) | b[4];
    } else {
      // 0x01 gesture, 0x02 finger count, 0x03..0x06 X high/low, Y high/low.
      if (!readRegs(kCstAddr, 0x01, b, 6) || (b[1] & 0x0F) == 0) return false;
      rawX = ((b[2] & 0x0F) << 8) | b[3];
      rawY = ((b[4] & 0x0F) << 8) | b[5];
    }
    if (TOUCH_SWAP_XY) {
      const int t = rawX;
      rawX = rawY;
      rawY = t;
    }
    if (TOUCH_FLIP_X) rawX = SCREEN_WIDTH_PX - 1 - rawX;
    if (TOUCH_FLIP_Y) rawY = SCREEN_HEIGHT_PX - 1 - rawY;
    x = constrain(rawX, 0, SCREEN_WIDTH_PX - 1);
    y = constrain(rawY, 0, SCREEN_HEIGHT_PX - 1);
    return true;
  }

private:
  static constexpr uint8_t kFtAddr = 0x38;
  static constexpr uint8_t kCstAddr = 0x15;

  Chip _chip = Chip::None;
  unsigned long _lastProbeMs = 0;

  static bool present(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
  }

  static bool writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
  }

  static bool readRegs(uint8_t addr, uint8_t reg, uint8_t *out, size_t n) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(addr, static_cast<uint8_t>(n)) != n) return false;
    for (size_t i = 0; i < n; i++) out[i] = Wire.read();
    return true;
  }

  void probe() {
    _lastProbeMs = millis();
    if (present(kFtAddr)) {
      _chip = Chip::FT3168;
      // Monitor mode: low power, wakes and reports on touch.
      writeReg(kFtAddr, 0xA5, 0x01);
    } else if (present(kCstAddr)) {
      _chip = Chip::CST820;
      // Keep it awake so polling always gets an answer.
      writeReg(kCstAddr, 0xFE, 0x01);
    }
    if (_chip != Chip::None) Serial.printf("[Touch] %s found\n", chipName());
  }
};
