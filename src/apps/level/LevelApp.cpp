#include "apps/level/LevelApp.h"

#include <Arduino.h>
#include <math.h>
#include <utility>

#include "Config.h"
#include "core/System.h"

namespace level_app {
namespace {

// Smoothed gravity in screen axes (g).
float gravX = 0, gravY = 0, gravZ = 1;
bool gravInit = false;
bool levelEdge = false;
// Zero offsets (degrees), saved in NVS: flat x/y, edge horizontal/vertical.
float zeroX = 0, zeroY = 0, zeroH = 0, zeroV = 0;
// Last raw (uncalibrated) reading, used by "Set zero".
LevelInfo levelRaw;

constexpr float kRadToDeg = 57.29578f;

} // namespace

void begin() {
  zeroX = sys::prefs().getFloat("lvx", 0);
  zeroY = sys::prefs().getFloat("lvy", 0);
  zeroH = sys::prefs().getFloat("lvh", 0);
  zeroV = sys::prefs().getFloat("lvv", 0);
}

void open() { gravInit = false; }

void read(LevelInfo &out) {
  out = LevelInfo();
  out.sensor = sys::imu().ready();
  out.calibrated = zeroX != 0 || zeroY != 0 || zeroH != 0 || zeroV != 0;
  if (!out.sensor) return;
  float ax, ay, az;
  bool ok;
  {
    sys::I2cLock lock;
    ok = sys::imu().accel(ax, ay, az);
  }
  if (!ok) return;
  if (LEVEL_SWAP_XY) std::swap(ax, ay);
  if (LEVEL_FLIP_X) ax = -ax;
  if (LEVEL_FLIP_Y) ay = -ay;
  // Light smoothing: steady bubble, still quick to follow.
  const float k = gravInit ? 0.3f : 1.0f;
  gravX += (ax - gravX) * k;
  gravY += (ay - gravY) * k;
  gravZ += (az - gravZ) * k;
  gravInit = true;
  const float n = sqrtf(gravX * gravX + gravY * gravY + gravZ * gravZ);
  if (n < 0.2f) return;
  const float gx = gravX / n, gy = gravY / n, gz = gravZ / n;
  // Flat or on an edge, with some hysteresis so it doesn't flip back and forth.
  if (levelEdge && fabsf(gz) > 0.75f) levelEdge = false;
  else if (!levelEdge && fabsf(gz) < 0.6f) levelEdge = true;
  out.edge = levelEdge;
  if (!levelEdge) {
    out.x = asinf(gx) * kRadToDeg;
    out.y = asinf(gy) * kRadToDeg;
  } else {
    out.vertical = fabsf(gx) > fabsf(gy);
    out.angle = out.vertical ? atan2f(gy * (gx >= 0 ? 1 : -1), fabsf(gx)) * kRadToDeg
                             : atan2f(gx * (gy >= 0 ? 1 : -1), fabsf(gy)) * kRadToDeg;
  }
  levelRaw = out;
  out.x -= zeroX;
  out.y -= zeroY;
  out.angle -= out.vertical ? zeroV : zeroH;

  static unsigned long lastLogMs = 0;
  if (millis() - lastLogMs > 1000) {
    lastLogMs = millis();
    Serial.printf("[Level] raw %.2f %.2f %.2f -> %s x=%.1f y=%.1f angle=%.1f\n", ax, ay, az,
                  out.edge ? (out.vertical ? "edge-v" : "edge-h") : "flat", out.x, out.y,
                  out.angle);
  }
}

// The current position counts as level from now on.
void setZero() {
  if (!levelRaw.edge) {
    zeroX = levelRaw.x;
    zeroY = levelRaw.y;
    sys::prefs().putFloat("lvx", zeroX);
    sys::prefs().putFloat("lvy", zeroY);
  } else if (levelRaw.vertical) {
    zeroV = levelRaw.angle;
    sys::prefs().putFloat("lvv", zeroV);
  } else {
    zeroH = levelRaw.angle;
    sys::prefs().putFloat("lvh", zeroH);
  }
  Serial.printf("[Level] zero set (%s)\n",
                levelRaw.edge ? (levelRaw.vertical ? "edge-v" : "edge-h") : "flat");
}

} // namespace level_app
