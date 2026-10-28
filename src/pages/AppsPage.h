#pragma once

// Apps: choose the app on the home screen.

#include "ui/Widgets.h"

namespace ui {

// ---------------------------------------------------------------------------
// Apps: which app is on the home screen
// ---------------------------------------------------------------------------
inline Rect appRowRect(int i) { return Rect{14, 84 + i * 72, 340, 62}; }


inline void drawAppsStatic(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  c.clear(kBackground);
  drawTitleBar(c, st.screen, i18n::tr(L, Str::Apps));
  for (int i = 0; i < kHomeAppCount; i++) {
    const Rect r = appRowRect(i);
    const bool on = static_cast<int>(st.app) == i;
    c.roundRect(r.x, r.y, r.w, r.h, 18, on ? rgb(0x1E3A33) : rgb(0x14171D));
    const float cx = r.x + 30, cy = r.y + r.h / 2.0f;
    c.ring(cx, cy, 11, 2.2f, on ? kAccent : kMuted);
    if (on) c.fillCircle(cx, cy, 6, kAccent);
    c.text(kFontBody, r.x + 56, r.y + r.h / 2 + 7, appName(L, static_cast<HomeApp>(i)), kText);
  }
  c.text(kFontSmall, W / 2, 418, i18n::tr(L, Str::AppsHint), kMuted, Align::Center);
  c.text(kFontSmall, W / 2, 440, i18n::tr(L, Str::SoundSoon), kFaint, Align::Center);
}

// Tapped app row, or -1.
inline int appAt(const UiState &st, int x, int y) {
  if (st.page != Page::Apps) return -1;
  for (int i = 0; i < kHomeAppCount; i++) {
    if (inside(appRowRect(i), x, y, 5)) return i;
  }
  return -1;
}

} // namespace ui
