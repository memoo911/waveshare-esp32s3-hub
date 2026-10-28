#pragma once

// Small building blocks shared by all pages: colours, hit testing, the title
// bar, the home-screen tile layout and the app names.

#include "ui/UiState.h"

namespace ui {

using screen::kAccent;
using screen::kBackground;
using screen::kFaint;
using screen::kMuted;
using screen::kText;
using screen::W;

// Extra margin around touch targets so a finger doesn't have to be exact.
constexpr int kTouchMargin = 14;

inline bool inside(const Rect &r, int x, int y, int margin = 0) {
  return x >= r.x - margin && y >= r.y - margin && x < r.x + r.w + margin &&
         y < r.y + r.h + margin;
}

constexpr Rgb kDanger = rgb(0xFF6B6B);

// Rounded "pill" button and a label centred on it.
inline void pill(Canvas &c, const Rect &r, Rgb bg) {
  c.roundRect(r.x, r.y, r.w, r.h, r.h / 2.0f, bg);
}

inline void pillLabel(Canvas &c, const Rect &r, const Font &f, const char *text, Rgb col,
                      int dy = 7) {
  c.text(f, r.x + r.w / 2, r.y + r.h / 2 + dy, text, col, Align::Center);
}

inline void drawTitleBar(Canvas &c, const ScreenState &s, const char *title) {
  screen::drawBackChevron(c, 16, 27, kText);
  c.text(kFontTitle, 34, 37, title, kText);
  screen::drawBattery(c, s);
}


// ---------------------------------------------------------------------------
// Home screen tile layout (shared by the apps)
// ---------------------------------------------------------------------------
constexpr Rect kWeatherTile{24, 196, 152, 168};
constexpr Rect kSettingsTile{192, 196, 152, 168};
constexpr int kTileRadius = 30;
constexpr int kTileLabelBaseline = 398;
// Animated mini weather icon inside the weather tile.
constexpr Rect kTileIconRect{36, 202, 128, 118};
constexpr float kTileIconCx = 100, kTileIconCy = 260, kTileIconSize = 96;


inline const char *appName(Lang L, HomeApp a) {
  switch (a) {
  case HomeApp::Timer: return i18n::tr(L, Str::KitchenTimer);
  case HomeApp::Level: return i18n::tr(L, Str::SpiritLevel);
  case HomeApp::Assistant: return i18n::tr(L, Str::AssistantApp);
  default: return i18n::tr(L, Str::Weather);
  }
}

} // namespace ui
