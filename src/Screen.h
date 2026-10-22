#pragma once

// Layout of the weather page (368x448 portrait AMOLED). Pure drawing: the
// caller fills a ScreenState and this paints into the canvas.
//
// Drawing is split in two layers so animation stays cheap:
// - drawStatic(): everything that changes at most once a minute or when data
//   arrives. Rendered into a cached base layer.
// - drawDynamic(): the animated weather icon and the blinking clock colon,
//   redrawn every frame over a copy of the base in their rectangles.

#include "Fonts.h"
#include "I18n.h"
#include "Icons.h"
#include "Render.h"
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

// Two dots at the top-right of the card: which weather page is showing.
inline void drawPageDots(Canvas &c, int active) {
  for (int i = 0; i < 2; i++) {
    const float x = 322 + i * 14;
    if (i == active) c.fillCircle(x, 202, 3.2f, kText);
    else c.fillCircle(x, 202, 3.2f, kFaint);
  }
}

inline void drawCard(Canvas &c, const ScreenState &s) {
  const WeatherData &w = s.weather;
  Rgb top, bottom;
  icons::tint(w.valid ? w.code : 3, w.valid ? w.isDay : true, top, bottom);
  c.roundRectGradient(14, 186, 340, 200, 26, top, bottom);
  drawPageDots(c, 0);

  if (!w.valid) {
    fitText(c, W / 2, 292, s.message ? s.message : i18n::tr(s.lang, Str::LoadingForecast),
            kMuted);
    return;
  }

  char buf[32];
  formatTemp(buf, sizeof(buf), w.temperature);
  c.text(kFontTemp, 190, 284, buf, kText);

  formatTemp(buf, sizeof(buf), w.feelsLike);
  char line[48];
  snprintf(line, sizeof(line), i18n::tr(s.lang, Str::FeelsLike), buf);
  c.text(kFontSmall, 194, 312, line, kMuted);

  if (w.isDay && w.sunset[0]) {
    snprintf(line, sizeof(line), i18n::tr(s.lang, Str::Sunset), w.sunset);
  } else if (!w.isDay && w.sunrise[0]) {
    snprintf(line, sizeof(line), i18n::tr(s.lang, Str::Sunrise), w.sunrise);
  } else {
    line[0] = 0;
  }
  if (line[0]) c.text(kFontSmall, 194, 334, line, kMuted);

  c.fillRect(38, 346, 292, 1, kFaint, 0.6f);
  c.text(kFontTitle, W / 2, 374, i18n::describe(s.lang, w.code, w.isDay), kText,
         Align::Center);
}

inline void drawStats(Canvas &c, const ScreenState &s) {
  const WeatherData &w = s.weather;
  if (!w.valid) return;
  const int cols[3] = {66, 184, 302};
  const char *labels[3] = {i18n::tr(s.lang, Str::MinMax), i18n::tr(s.lang, Str::Humidity),
                           i18n::tr(s.lang, Str::Wind)};
  char values[3][24];
  char lo[8], hi[8];
  formatTemp(lo, sizeof(lo), w.todayMin);
  formatTemp(hi, sizeof(hi), w.todayMax);
  snprintf(values[0], sizeof(values[0]), "%s / %s", lo, hi);
  snprintf(values[1], sizeof(values[1]), "%d %%", w.humidity);
  snprintf(values[2], sizeof(values[2]), "%d km/h", static_cast<int>(lroundf(w.windKmh)));
  for (int i = 0; i < 3; i++) {
    c.text(kFontSmall, cols[i], 409, labels[i], kMuted, Align::Center);
    c.text(kFontBody, cols[i], 433, values[i], kText, Align::Center);
  }
  c.fillRect(125, 398, 1, 38, kFaint);
  c.fillRect(243, 398, 1, 38, kFaint);
}

inline Rect iconRect() { return Rect{kIconX, kIconY, kIconW, kIconH}; }

inline void drawIcon(Canvas &c, const ScreenState &s) {
  const WeatherData &w = s.weather;
  if (!w.valid) return;
  icons::weather(c, kIconX + kIconW / 2, kIconY + kIconH / 2 + 2, 150, w.code,
                 w.isDay, s.moonPhase, s.animTime);
}

inline void drawStatic(Canvas &c, const ScreenState &s) {
  c.clear(kBackground);
  drawTopBar(c, s);
  drawClock(c, s);
  drawCard(c, s);
  drawStats(c, s);
}

inline void drawDynamic(Canvas &c, const ScreenState &s) {
  drawColon(c, s);
  drawIcon(c, s);
}

// ---------------------------------------------------------------------------
// 3-day forecast page
// ---------------------------------------------------------------------------
constexpr int kForecastTop = 192;
constexpr int kForecastRowH = 80;
constexpr float kForecastIconX = 176, kForecastIconSize = 60;

inline Rect forecastIconRect(int i) {
  const int cy = kForecastTop + i * kForecastRowH + 40;
  return Rect{static_cast<int>(kForecastIconX) - 36, cy - 34, 72, 70};
}

inline void drawRaindrop(Canvas &c, float x, float y, Rgb col) {
  c.fillCircle(x, y + 2, 3.6f, col);
  const float pts[] = {x - 3.4f, y + 1, x + 3.4f, y + 1, x, y - 6};
  c.polygon(pts, 3, col);
}

inline void drawForecastStatic(Canvas &c, const ScreenState &s) {
  c.clear(kBackground);
  drawTopBar(c, s);
  drawClock(c, s);
  c.roundRectGradient(14, 186, 340, 250, 26, rgb(0x171B23), rgb(0x0E1014));
  drawPageDots(c, 1);
  const WeatherData &w = s.weather;
  if (!w.valid || w.dayCount == 0) {
    c.text(kFontBody, W / 2, 316, i18n::tr(s.lang, Str::ForecastNotYet), kMuted,
           Align::Center);
    return;
  }
  for (int i = 0; i < w.dayCount && i < 3; i++) {
    const DayForecast &d = w.days[i];
    const int y0 = kForecastTop + i * kForecastRowH;
    c.text(kFontBody, 32, y0 + 36,
           i == 0 ? i18n::tr(s.lang, Str::Tomorrow) : i18n::weekday(s.lang, d.weekday), kText);
    char buf[24];
    if (s.lang == Lang::Hr) {
      snprintf(buf, sizeof(buf), "%d. %d.", d.day, d.month);
    } else {
      snprintf(buf, sizeof(buf), "%d/%d", d.month, d.day);
    }
    c.text(kFontSmall, 32, y0 + 58, buf, kMuted);
    if (d.precipChance >= 0) {
      drawRaindrop(c, 222, y0 + 44, icons::kRain);
      snprintf(buf, sizeof(buf), "%d %%", d.precipChance);
      c.text(kFontSmall, 232, y0 + 50, buf, kMuted);
    }
    formatTemp(buf, sizeof(buf), d.tempMax);
    c.text(kFontTitle, 334, y0 + 38, buf, kText, Align::Right);
    formatTemp(buf, sizeof(buf), d.tempMin);
    c.text(kFontSmall, 334, y0 + 60, buf, kMuted, Align::Right);
    if (i < 2) c.fillRect(32, y0 + kForecastRowH, 304, 1, kFaint, 0.6f);
  }
}

inline void drawForecastDynamic(Canvas &c, const ScreenState &s) {
  drawColon(c, s);
  const WeatherData &w = s.weather;
  if (!w.valid) return;
  for (int i = 0; i < w.dayCount && i < 3; i++) {
    // Forecast days use the daytime icon.
    icons::weather(c, kForecastIconX, kForecastTop + i * kForecastRowH + 40,
                   kForecastIconSize, w.days[i].code, true, s.moonPhase, s.animTime);
  }
}

// Whole page in one go (used by the PC preview).
inline void draw(Canvas &c, const ScreenState &s) {
  drawStatic(c, s);
  drawDynamic(c, s);
}

} // namespace screen
