#pragma once

// Home screen: clock, the chosen app's tile and Settings.

#include "apps/assistant/AssistantUi.h"
#include "apps/level/LevelUi.h"
#include "apps/timer/TimerUi.h"
#include "apps/weather/WeatherUi.h"

namespace ui {

// ---------------------------------------------------------------------------
// Home
// ---------------------------------------------------------------------------
inline void gear(Canvas &c, float cx, float cy, Rgb col, Rgb hole) {
  for (int i = 0; i < 8; i++) {
    const float a = i * 3.14159265f / 4 + 0.39f;
    c.capsule(cx + cosf(a) * 22, cy + sinf(a) * 22, cx + cosf(a) * 31,
              cy + sinf(a) * 31, 6.0f, col);
  }
  c.fillCircle(cx, cy, 25, col);
  c.fillCircle(cx, cy, 10, hole);
}


inline void drawHomeStatic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  c.clear(kBackground);
  screen::drawTopBar(c, s);
  screen::drawClock(c, s);

  if (st.app == HomeApp::Timer) drawTimerTile(c, st);
  else if (st.app == HomeApp::Level) drawLevelTile(c, st);
  else if (st.app == HomeApp::Assistant) drawAssistantTile(c, st);
  else drawWeatherTile(c, st);

  // Settings tile.
  const Rect &st2 = kSettingsTile;
  const Rgb sTop = rgb(0x262B34), sBottom = rgb(0x14171C);
  c.roundRectGradient(st2.x, st2.y, st2.w, st2.h, kTileRadius, sTop, sBottom);
  gear(c, st2.x + st2.w / 2, st2.y + st2.h / 2, rgb(0xC9D1DC), mix(sTop, sBottom, 0.5f));
  c.text(kFontBody, st2.x + st2.w / 2, kTileLabelBaseline, i18n::tr(s.lang, Str::Settings),
         kText, Align::Center);
}

inline void drawHomeDynamic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  screen::drawColon(c, s);
  if (st.app == HomeApp::Weather && s.weather.valid) {
    icons::weather(c, kTileIconCx, kTileIconCy, kTileIconSize, s.weather.code,
                   s.weather.isDay, s.moonPhase, s.animTime);
  }
}

} // namespace ui
