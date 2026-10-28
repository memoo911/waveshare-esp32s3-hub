#pragma once

// Turn off: confirmation page.

#include "ui/Widgets.h"

namespace ui {

// ---------------------------------------------------------------------------
// Turn off: confirmation with big buttons
// ---------------------------------------------------------------------------
constexpr Rect kPowerOffButton{44, 300, 280, 64};
constexpr Rect kCancelButton{44, 378, 280, 52};

inline void drawPowerOffStatic(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  c.clear(kBackground);
  drawTitleBar(c, st.screen, i18n::tr(L, Str::TurnOff));
  // Power symbol: open ring with a bar through the gap.
  const float cx = W / 2.0f, cy = 156, r = 40;
  c.ring(cx, cy, r, 7, kDanger);
  c.fillRect(static_cast<int>(cx - 16), static_cast<int>(cy - r - 8), 32, 30, kBackground);
  c.capsule(cx, cy - r - 4, cx, cy - 6, 3.5f, kDanger);
  c.text(kFontBody, W / 2, 240, i18n::tr(L, Str::TurnOffQuestion), kText, Align::Center);
  c.text(kFontSmall, W / 2, 268, i18n::tr(L, Str::TurnOnHint), kMuted, Align::Center);
  const Rect &b = kPowerOffButton;
  c.roundRect(b.x, b.y, b.w, b.h, b.h / 2, kDanger);
  c.text(kFontBody, b.x + b.w / 2, b.y + b.h / 2 + 8, i18n::tr(L, Str::TurnOff),
         rgb(0x000000), Align::Center);
  const Rect &k = kCancelButton;
  c.roundRect(k.x, k.y, k.w, k.h, k.h / 2, rgb(0x14171D));
  c.text(kFontBody, k.x + k.w / 2, k.y + k.h / 2 + 8, i18n::tr(L, Str::Cancel), kText,
         Align::Center);
}

} // namespace ui
