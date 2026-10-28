#pragma once

// Display: brightness slider, screen-off time and night dimming. The slider and
// option-row layout here is shared with the Sound page.

#include "ui/Widgets.h"

namespace ui {

// ---------------------------------------------------------------------------
// Display: brightness slider and when the screen turns off
// ---------------------------------------------------------------------------
constexpr int kSliderX = 44, kSliderW = 280, kSliderY = 214;
constexpr int kBrightnessMin = 5, kBrightnessMax = 100;
// Area that reacts to a finger: the slider plus generous room around it.
constexpr Rect kSliderTouch{0, kSliderY - 48, W, 92};
// Value and slider are redrawn every frame so dragging feels immediate.
constexpr Rect kBrightnessDynamic{0, 68, W, 168};
constexpr int kValueBaseline = 170;
// Big rows under the slider (Display: screen off, night dimming; Sound:
// hourly chime). Their tap areas meet halfway between them and reach the
// bottom of the screen, so they are easy to hit.
constexpr Rect kOptionRow{14, 274, 340, 60};
constexpr Rect kOption2Row{14, 344, 340, 60};
constexpr int kOptionsTop = kSliderTouch.y + kSliderTouch.h + 4;
constexpr int kOptionsSplit = (kOptionRow.y + kOptionRow.h + kOption2Row.y) / 2;

// Screen-off choices in minutes; 0 = never.
constexpr int kScreenOffMinutes[] = {1, 2, 5, 0};
constexpr int kScreenOffChoices = 4;

inline int sliderValueAt(int x) {
  const float t = clamp01(static_cast<float>(x - kSliderX) / kSliderW);
  return kBrightnessMin + static_cast<int>(lroundf(t * (kBrightnessMax - kBrightnessMin)));
}

inline void sunGlyph(Canvas &c, float cx, float cy, float r, Rgb col) {
  for (int i = 0; i < 8; i++) {
    const float a = i * 3.14159265f / 4;
    c.capsule(cx + cosf(a) * r * 1.5f, cy + sinf(a) * r * 1.5f, cx + cosf(a) * r * 2.0f,
              cy + sinf(a) * r * 2.0f, r * 0.22f, col);
  }
  c.fillCircle(cx, cy, r, col);
}

inline void drawSwitch(Canvas &c, int right, int cy, bool on) {
  const int w = 52, h = 30;
  const int x = right - w;
  c.capsule(x + h / 2, cy, x + w - h / 2, cy, h / 2.0f, on ? kAccent : rgb(0x2A2F38));
  c.fillCircle(on ? x + w - h / 2 : x + h / 2, cy, h / 2.0f - 4, on ? kText : kMuted);
}

inline void screenOffText(Lang L, int index, char *out, size_t n) {
  const int m = kScreenOffMinutes[(index % kScreenOffChoices + kScreenOffChoices) %
                                  kScreenOffChoices];
  if (m == 0) snprintf(out, n, "%s", i18n::tr(L, Str::Never));
  else snprintf(out, n, "%d min", m);
}

inline void drawBrightnessStatic(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  c.clear(kBackground);
  drawTitleBar(c, st.screen, i18n::tr(L, Str::Brightness));
  sunGlyph(c, kSliderX - 2, kSliderY + 34, 4, kMuted);
  sunGlyph(c, kSliderX + kSliderW - 2, kSliderY + 34, 6, kText);
  const Rect r = kOptionRow;
  c.roundRect(r.x, r.y, r.w, r.h, 18, rgb(0x14171D));
  c.text(kFontBody, 34, r.y + r.h / 2 + 7, i18n::tr(L, Str::ScreenOffAfter), kText);
  char buf[24];
  screenOffText(L, st.info.screenOff, buf, sizeof(buf));
  c.text(kFontSmall, r.x + r.w - 20, r.y + r.h / 2 + 6, buf, kAccent, Align::Right);
  const Rect n = kOption2Row;
  c.roundRect(n.x, n.y, n.w, n.h, 18, rgb(0x14171D));
  c.text(kFontBody, 34, n.y + n.h / 2 + 7, i18n::tr(L, Str::NightDimming), kText);
  drawSwitch(c, n.x + n.w - 18, n.y + n.h / 2, st.info.nightDim);
  c.text(kFontSmall, W / 2, 428, i18n::tr(L, Str::NightHint), kMuted, Align::Center);
}

inline void drawBrightnessDynamic(Canvas &c, const UiState &st) {
  const int v = st.info.brightnessPercent;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d %%", v);
  c.text(kFontTemp, W / 2, kValueBaseline, buf, kText, Align::Center);
  const float t = static_cast<float>(v - kBrightnessMin) / (kBrightnessMax - kBrightnessMin);
  const float kx = kSliderX + t * kSliderW;
  c.capsule(kSliderX, kSliderY, kSliderX + kSliderW, kSliderY, 5, rgb(0x2A2F38));
  c.capsule(kSliderX, kSliderY, kx, kSliderY, 5, kAccent);
  c.fillCircle(kx, kSliderY, 15, kText);
}

} // namespace ui
