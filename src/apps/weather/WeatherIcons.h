#pragma once

// Animated weather icons drawn from shapes, for WMO weather codes as returned
// by Open-Meteo. Everything is a function of time `t` (seconds), so the icon
// animates simply by being redrawn: rain falls, snow drifts, stars twinkle,
// the sun's rays turn slowly and lightning flashes now and then.
//
// Icons are designed in a 100x100 box around (cx, cy) and scaled by size/100.

#include "gfx/Render.h"

namespace icons {

constexpr Rgb kSun = rgb(0xFFC93C);
constexpr Rgb kSunGlow = rgb(0xFF9F1C);
constexpr Rgb kMoonLit = rgb(0xF2EEDF);
constexpr Rgb kMoonDark = rgb(0x262A33);
constexpr Rgb kMoonCrater = rgb(0xD9D3C1);
constexpr Rgb kCloudLight = rgb(0xE9EDF2);
constexpr Rgb kCloudShade = rgb(0xB8C0CC);
constexpr Rgb kCloudDark = rgb(0x8F98A6);
constexpr Rgb kCloudStorm = rgb(0x6C7380);
constexpr Rgb kRain = rgb(0x5AB8FF);
constexpr Rgb kSnow = rgb(0xFFFFFF);
constexpr Rgb kBolt = rgb(0xFFD84D);
constexpr Rgb kFog = rgb(0xAEB6C2);
constexpr Rgb kStar = rgb(0xFFF6D8);

// Moon phase for a Unix time: 0 = new, 0.25 = first quarter, 0.5 = full,
// 0.75 = last quarter. Based on the new moon of 2000-01-06 18:14 UTC.
inline float moonPhase(double unixSeconds) {
  const double days = (unixSeconds - 947182440.0) / 86400.0;
  double p = fmod(days / 29.530588853, 1.0);
  if (p < 0) p += 1.0;
  return static_cast<float>(p);
}

inline float frac(float v) { return v - floorf(v); }

inline void sun(Canvas &c, float cx, float cy, float s, float t) {
  c.glow(cx, cy, 40 * s, kSunGlow, 0.22f);
  const float spin = t * 0.25f;
  for (int i = 0; i < 8; i++) {
    const float a = spin + i * 3.14159265f / 4;
    const float pulse = 1 + 0.06f * sinf(t * 2 + i);
    c.capsule(cx + cosf(a) * 25 * s, cy + sinf(a) * 25 * s,
              cx + cosf(a) * 33 * s * pulse, cy + sinf(a) * 33 * s * pulse,
              2.6f * s, kSun);
  }
  c.fillCircle(cx, cy, 18 * s, kSun);
}

// Moon showing the real phase. The unlit part is drawn faintly so the full
// disc stays readable, like earthshine.
inline void moon(Canvas &c, float cx, float cy, float r, float phase) {
  c.glow(cx, cy, r * 1.9f, kMoonLit, 0.10f);
  const float k = cosf(phase * 2 * 3.14159265f);
  const bool waxing = phase < 0.5f;
  // Craters in disc units (x, y, radius).
  static const float craters[][3] = {
      {-0.30f, -0.25f, 0.18f}, {0.25f, 0.30f, 0.14f}, {0.35f, -0.35f, 0.10f},
      {-0.15f, 0.40f, 0.09f}, {0.05f, -0.05f, 0.07f}};
  const int x0 = static_cast<int>(cx - r) - 1, x1 = static_cast<int>(cx + r) + 1;
  const int y0 = static_cast<int>(cy - r) - 1, y1 = static_cast<int>(cy + r) + 1;
  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      const float px = x + 0.5f - cx, py = y + 0.5f - cy;
      const float dist = sqrtf(px * px + py * py);
      const float disc = clamp01(r - dist + 0.5f);
      if (disc <= 0) continue;
      const float nx = px / r, ny = py / r;
      const float half = sqrtf(fmaxf(0.0f, 1 - ny * ny));
      // Signed distance (in pixels) to the terminator; positive = lit.
      const float edge = waxing ? (nx - k * half) * r : (-k * half - nx) * r;
      const float lit = clamp01(edge * 0.8f + 0.5f);
      Rgb col = kMoonLit;
      for (const auto &cr : craters) {
        const float dx = nx - cr[0], dy = ny - cr[1];
        if (dx * dx + dy * dy < cr[2] * cr[2]) col = kMoonCrater;
      }
      c.blend(x, y, mix(kMoonDark, col, lit), disc);
    }
  }
}

inline void stars(Canvas &c, float cx, float cy, float s, float t) {
  static const float pos[][2] = {{-38, -30}, {32, -38}, {40, 8}, {-42, 18}, {-8, -44}};
  for (int i = 0; i < 5; i++) {
    const float tw = 0.35f + 0.65f * (0.5f + 0.5f * sinf(t * (1.3f + i * 0.4f) + i * 2));
    const float x = cx + pos[i][0] * s, y = cy + pos[i][1] * s;
    c.glow(x, y, 5 * s, kStar, 0.5f * tw);
    c.fillCircle(x, y, 1.4f * s, kStar, tw);
  }
}

// Cloud made of overlapping circles on a rounded base, with a darker
// underside. `shade` and `body` set its colors.
inline void cloud(Canvas &c, float cx, float cy, float s, Rgb body, Rgb shade,
                  float a = 1) {
  // Underside, shifted down slightly.
  c.roundRect(cx - 33 * s, cy + 2 * s, 68 * s, 21 * s, 10.5f * s, shade, a);
  c.fillCircle(cx - 18 * s, cy + 6 * s, 14 * s, shade, a);
  c.fillCircle(cx + 20 * s, cy + 6 * s, 15 * s, shade, a);
  // Body.
  c.roundRect(cx - 33 * s, cy, 68 * s, 20 * s, 10 * s, body, a);
  c.fillCircle(cx - 18 * s, cy + 3 * s, 14 * s, body, a);
  c.fillCircle(cx + 1 * s, cy - 8 * s, 20 * s, body, a);
  c.fillCircle(cx + 20 * s, cy + 3 * s, 15 * s, body, a);
}

// Falling streaks below a cloud. `count` drops, `speed` in box units/second.
inline void rain(Canvas &c, float cx, float cy, float s, float t, int count,
                 float speed, Rgb col, float len) {
  for (int i = 0; i < count; i++) {
    const float lane = (i - (count - 1) / 2.0f) * (58.0f / count);
    const float p = frac(t * speed / 30 + i * 0.37f);
    const float y = cy + 26 * s + p * 30 * s;
    const float x = cx + lane * s - p * 6 * s;
    const float fade = sinf(p * 3.14159265f);
    c.capsule(x, y, x - 2.5f * s, y + len * s, 2.0f * s, col, fade);
  }
}

inline void snow(Canvas &c, float cx, float cy, float s, float t, int count) {
  for (int i = 0; i < count; i++) {
    const float lane = (i - (count - 1) / 2.0f) * (60.0f / count);
    const float p = frac(t * 0.35f + i * 0.29f);
    const float y = cy + 26 * s + p * 30 * s;
    const float x = cx + lane * s + sinf(t * 1.7f + i) * 3 * s;
    const float fade = sinf(p * 3.14159265f);
    c.fillCircle(x, y, 2.6f * s, kSnow, fade);
  }
}

inline void bolt(Canvas &c, float cx, float cy, float s, float t) {
  // Flash twice every four seconds, otherwise glow faintly.
  const float ph = fmodf(t, 4.0f);
  const bool flash = (ph < 0.12f) || (ph > 0.25f && ph < 0.35f);
  const float a = flash ? 1.0f : 0.55f;
  const float pts[] = {
      cx + 4 * s, cy + 22 * s, cx - 8 * s, cy + 40 * s, cx + 0 * s, cy + 40 * s,
      cx - 6 * s, cy + 56 * s, cx + 12 * s, cy + 34 * s, cx + 4 * s, cy + 34 * s,
      cx + 10 * s, cy + 22 * s};
  if (flash) c.glow(cx, cy + 38 * s, 30 * s, kBolt, 0.35f);
  c.polygon(pts, 7, kBolt, a);
}

inline void fog(Canvas &c, float cx, float cy, float s, float t) {
  for (int i = 0; i < 4; i++) {
    const float y = cy - 12 * s + i * 11 * s;
    const float drift = sinf(t * 0.6f + i * 1.3f) * 6 * s;
    const float half = (i % 2 == 0 ? 30 : 24) * s;
    c.capsule(cx - half + drift, y, cx + half + drift, y, 3.2f * s, kFog,
              0.85f - i * 0.12f);
  }
}

// Sun by day, real-phase moon with stars by night.
// Stars are only drawn when `withStars`; callers that offset the sky body
// draw them around the icon center instead, so they stay inside the card.
inline void sky(Canvas &c, float cx, float cy, float s, bool day, float phase,
                float t, float bodyScale, bool withStars = true) {
  if (day) {
    sun(c, cx, cy, s * bodyScale, t);
  } else {
    if (withStars) stars(c, cx, cy, s, t);
    moon(c, cx, cy, 22 * s * bodyScale, phase);
  }
}

// Draw the icon for WMO weather code `code` centered at (cx, cy).
inline void weather(Canvas &c, float cx, float cy, float size, int code,
                    bool day, float phase, float t) {
  const float s = size / 100.0f;
  if (code < 0) return;

  if (code == 0) {
    sky(c, cx, cy, s, day, phase, t, 1.15f);
    return;
  }
  if (code == 1 || code == 2) {
    const float cloudScale = code == 1 ? 0.62f : 0.82f;
    if (!day) stars(c, cx, cy, s * 0.9f, t);
    sky(c, cx - 14 * s, cy - 14 * s, s, day, phase, t, 0.85f, false);
    cloud(c, cx + 10 * s, cy + 14 * s, s * cloudScale, kCloudLight, kCloudShade);
    return;
  }
  if (code == 3) {
    cloud(c, cx - 14 * s, cy - 12 * s, s * 0.7f, kCloudShade, kCloudDark);
    cloud(c, cx + 6 * s, cy + 6 * s, s, kCloudLight, kCloudShade);
    return;
  }
  if (code == 45 || code == 48) {
    cloud(c, cx, cy - 16 * s, s * 0.8f, kCloudShade, kCloudDark);
    fog(c, cx, cy + 22 * s, s, t);
    return;
  }

  const bool snowy = (code >= 71 && code <= 77) || code == 85 || code == 86;
  const bool freezing = code == 56 || code == 57 || code == 66 || code == 67;
  const bool storm = code >= 95;
  const bool drizzle = code >= 51 && code <= 57;
  const bool heavy = code == 65 || code == 67 || code == 75 || code == 82 ||
                     code == 86 || code == 99;

  const float ccy = cy - 14 * s;
  const Rgb body = storm ? kCloudDark : (heavy ? kCloudShade : kCloudLight);
  const Rgb shade = storm ? kCloudStorm : (heavy ? kCloudDark : kCloudShade);

  if (snowy) {
    snow(c, cx, ccy, s, t, heavy ? 6 : 4);
  } else if (storm) {
    rain(c, cx, ccy, s, t, 4, 70, kRain, 9);
    bolt(c, cx, ccy, s, t);
  } else if (drizzle) {
    rain(c, cx, ccy, s, t, 5, 40, kRain, 4);
  } else {
    rain(c, cx, ccy, s, t, heavy ? 7 : 5, heavy ? 85 : 65, kRain, heavy ? 11 : 8);
  }
  if (freezing) snow(c, cx, ccy, s, t + 0.7f, 3);
  cloud(c, cx, ccy, s, body, shade);
}

// Whether the icon for this code moves (and needs redrawing every frame).
// Overcast is a still pair of clouds.
inline bool isAnimated(int code) { return code >= 0 && code != 3; }

// Card background tint for the current conditions (top, bottom).
inline void tint(int code, bool day, Rgb &top, Rgb &bottom) {
  bottom = rgb(0x0E1014);
  if (code >= 95) top = rgb(0x221A33);
  else if ((code >= 71 && code <= 77) || code == 85 || code == 86) top = rgb(0x1B2430);
  else if (code >= 45) top = rgb(0x12202F);
  else if (code == 3) top = rgb(0x1A1D23);
  else if (day) top = rgb(0x2B210E);
  else top = rgb(0x111A33);
}

} // namespace icons
