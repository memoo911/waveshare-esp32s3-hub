#pragma once

// Layout of the weather page (368x448 portrait AMOLED). Pure drawing: the
// caller fills a ScreenState and this paints into the canvas.
//
// Drawing is split in two layers so animation stays cheap:
// - drawStatic(): everything that changes at most once a minute or when data
//   arrives. Rendered into a cached base layer.
// - drawDynamic(): the animated weather icon and the blinking clock colon,
//   redrawn every frame over a copy of the base in their rectangles.

#include "core/I18n.h"
#include "gfx/Fonts.h"
#include "gfx/Render.h"
#include <stdio.h>

// One day of the daily forecast.
struct DayForecast {
  int code = -1;
  float tempMax = 0;
  float tempMin = 0;
  int precipChance = -1; // percent, -1 = unknown
  int weekday = 0;       // 0 = Sunday
  int day = 0, month = 0;
};

struct WeatherData {
  bool valid = false;
  float temperature = 0;
  float feelsLike = 0;
  int humidity = 0;
  float windKmh = 0;
  int code = -1;
  bool isDay = true;
  float todayMin = 0;
  float todayMax = 0;
  char sunrise[6] = "";
  char sunset[6] = "";
  DayForecast days[3];   // tomorrow and the two days after
  int dayCount = 0;
};

struct ScreenState {
  const char *city = "Zagreb";
  Lang lang = Lang::En;

  bool timeValid = false;
  int hour = 0, minute = 0, second = 0;
  int weekday = 0; // 0 = Sunday
  int day = 1, month = 1;

  WeatherData weather;
  bool weatherStale = false;   // last refresh failed or data is old
  const char *message = nullptr; // shown instead of the weather card

  int batteryPercent = -1;     // -1 = no battery / unknown
  bool charging = false;
  bool wifiConnected = false;

  float moonPhase = 0;
  float animTime = 0;          // seconds, drives icon animation

  bool showBack = false;       // "‹" before the city: tap to go back home
  const char *toast = nullptr; // short notice in the top bar ("Osvježavam…")
};

struct Rect {
  int x, y, w, h;
};

namespace screen {

constexpr int W = 368;
constexpr int H = 448;

constexpr Rgb kBackground = rgb(0x000000);
constexpr Rgb kText = rgb(0xF3F5F8);
constexpr Rgb kMuted = rgb(0x8C94A1);
constexpr Rgb kFaint = rgb(0x4A515C);
constexpr Rgb kAccent = rgb(0x6FD3B1);
constexpr Rgb kWarn = rgb(0xFF8A65);
constexpr Rgb kBatteryOk = rgb(0x6FD38A);
constexpr Rgb kBatteryMid = rgb(0xF2C14E);
constexpr Rgb kBatteryLow = rgb(0xFF6B6B);

// Region that changes every animation frame (the weather icon).
constexpr int kIconX = 22, kIconY = 188, kIconW = 160, kIconH = 146;

// Centered message: the body font if it fits in `maxWidth`, else the small
// one (long network or place names).
inline void fitText(Canvas &c, int cx, int baseline, const char *text, Rgb col,
                    int maxWidth = 316) {
  const bool fits = c.textWidth(kFontBody, text) <= maxWidth;
  c.text(fits ? kFontBody : kFontSmall, cx, baseline, text, col, Align::Center);
}

// Integer temperature with a degree sign, avoiding "-0°".
inline void formatTemp(char *out, size_t n, float t) {
  int v = static_cast<int>(lroundf(t));
  if (v == 0) v = 0;
  snprintf(out, n, "%d°", v);
}

inline void drawBattery(Canvas &c, const ScreenState &s) {
  const int x = 318, y = 22, w = 30, h = 15;
  if (s.batteryPercent < 0) {
    // No battery: show only a plug-style dot if powered.
    c.text(kFontSmall, x + w, y + 13, "USB", kMuted, Align::Right);
    return;
  }
  const int pct = s.batteryPercent > 100 ? 100 : s.batteryPercent;
  Rgb fill = kBatteryOk;
  if (pct <= 15) fill = kBatteryLow;
  else if (pct <= 40) fill = kBatteryMid;
  if (s.charging) fill = kAccent;

  c.roundRect(x, y, w, h, 4, kMuted);
  c.roundRect(x + 1.5f, y + 1.5f, w - 3, h - 3, 2.8f, kBackground);
  c.roundRect(x + w + 1, y + 4.5f, 2.5f, h - 9, 1.2f, kMuted);
  const float inner = (w - 6) * pct / 100.0f;
  if (inner > 0.5f) c.roundRect(x + 3, y + 3, inner, h - 6, 1.8f, fill);
  if (s.charging) {
    const float bx = x + w / 2.0f, by = y + h / 2.0f;
    const float pts[] = {bx + 1.5f, by - 6, bx - 4, by + 1, bx - 0.5f, by + 1,
                         bx - 1.5f, by + 6, bx + 4, by - 1, bx + 0.5f, by - 1};
    c.polygon(pts, 6, kText);
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  c.text(kFontSmall, x - 8, y + 13, buf, kText, Align::Right);
}

// "‹" back chevron at the top-left.
inline void drawBackChevron(Canvas &c, float x, float y, Rgb col) {
  c.capsule(x + 6, y - 7, x, y, 1.6f, col);
  c.capsule(x, y, x + 6, y + 7, 1.6f, col);
}

inline void drawToast(Canvas &c, const char *text) {
  const int w = c.textWidth(kFontSmall, text) + 28;
  c.roundRect(W / 2 - w / 2, 14, w, 28, 14, rgb(0x1E3A33));
  c.text(kFontSmall, W / 2, 34, text, kAccent, Align::Center);
}

inline void drawTopBar(Canvas &c, const ScreenState &s) {
  int px = 30;
  if (s.showBack) {
    drawBackChevron(c, 16, 27, kText);
    px = 42;
  }
  // Location pin + city.
  c.fillCircle(px, 26, 5.5f, kAccent);
  c.fillCircle(px, 26, 2.2f, kBackground);
  const float pin[] = {px - 4.8f, 28.5f, px + 4.8f, 28.5f, static_cast<float>(px), 36};
  c.polygon(pin, 3, kAccent);
  int x = c.text(kFontSmall, px + 12, 35, s.city, kText);
  if (!s.wifiConnected) {
    c.text(kFontSmall, x + 10, 35, i18n::tr(s.lang, Str::NoWifi), kWarn);
  } else if (s.weatherStale) {
    c.text(kFontSmall, x + 10, 35, i18n::tr(s.lang, Str::Outdated), kWarn);
  }
  drawBattery(c, s);
  if (s.toast) drawToast(c, s.toast);
}

constexpr int kClockBaseline = 136;

// Clock layout: digits are tabular, so the colon never moves.
inline int clockColonX(const Canvas &c) {
  const int colonW = c.textWidth(kFontClock, ":");
  const int hw = c.textWidth(kFontClock, "00", true);
  const int mw = c.textWidth(kFontClock, "00", true);
  return W / 2 - (hw + colonW + mw) / 2 + hw;
}

inline Rect colonRect(const Canvas &c) {
  const int x = clockColonX(c);
  return Rect{x - 4, kClockBaseline - 90, c.textWidth(kFontClock, ":") + 8, 96};
}

// The colon breathes once per second (dynamic layer).
inline void drawColon(Canvas &c, const ScreenState &s) {
  const float a = s.timeValid ? (s.second % 2 == 0 ? 1.0f : 0.35f) : 0.5f;
  c.text(kFontClock, clockColonX(c), kClockBaseline, ":", kText, Align::Left, a);
}

// Hours, minutes and date (static layer; the colon is drawn separately).
inline void drawClock(Canvas &c, const ScreenState &s) {
  char hh[3], mm[3];
  if (s.timeValid) {
    snprintf(hh, sizeof(hh), "%02d", s.hour);
    snprintf(mm, sizeof(mm), "%02d", s.minute);
  } else {
    snprintf(hh, sizeof(hh), "--");
    snprintf(mm, sizeof(mm), "--");
  }
  const int cx = clockColonX(c);
  c.text(kFontClock, cx, kClockBaseline, hh, kText, Align::Right, 1, true);
  c.text(kFontClock, cx + c.textWidth(kFontClock, ":"), kClockBaseline, mm, kText,
         Align::Left, 1, true);

  if (s.timeValid) {
    char date[48];
    if (s.lang == Lang::Hr) {
      snprintf(date, sizeof(date), "%s, %d. %s", i18n::weekday(s.lang, s.weekday), s.day,
               i18n::month(s.lang, s.month));
    } else {
      snprintf(date, sizeof(date), "%s, %d %s", i18n::weekday(s.lang, s.weekday), s.day,
               i18n::month(s.lang, s.month));
    }
    c.text(kFontBody, W / 2, 170, date, kMuted, Align::Center);
  }
}

} // namespace screen
