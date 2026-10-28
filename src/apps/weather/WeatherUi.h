#pragma once

// Weather app: the current-weather card and the 3-day forecast (namespace
// screen), plus its tile on the home screen.

#include "apps/weather/WeatherIcons.h"
#include "ui/Widgets.h"

namespace screen {

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

namespace ui {

inline void drawWeatherTile(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  // Weather tile, tinted like the weather card.
  const WeatherData &w = s.weather;
  Rgb top, bottom;
  icons::tint(w.valid ? w.code : 3, w.valid ? w.isDay : true, top, bottom);
  const Rect &wt = kWeatherTile;
  c.roundRectGradient(wt.x, wt.y, wt.w, wt.h, kTileRadius, top, bottom);
  char temp[16];
  if (w.valid) {
    screen::formatTemp(temp, sizeof(temp), w.temperature);
  } else {
    snprintf(temp, sizeof(temp), "--");
  }
  c.text(kFontTitle, wt.x + wt.w / 2, wt.y + wt.h - 18, temp, kText, Align::Center);
  c.text(kFontBody, wt.x + wt.w / 2, kTileLabelBaseline, i18n::tr(s.lang, Str::Weather),
         kText, Align::Center);
}

} // namespace ui
