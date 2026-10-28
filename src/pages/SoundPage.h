#pragma once

// Sound: volume slider and the hourly chime.

#include "pages/DisplayPage.h"

namespace ui {

// ---------------------------------------------------------------------------
// Sound: volume slider and the hourly chime switch
// ---------------------------------------------------------------------------
constexpr int kVolumeSliderY = kSliderY;
constexpr Rect kVolumeTouch = kSliderTouch;
constexpr Rect kVolumeDynamic = kBrightnessDynamic;
constexpr Rect kChimeRow = kOptionRow;

inline int volumeAt(int x) {
  const float t = clamp01(static_cast<float>(x - kSliderX) / kSliderW);
  return static_cast<int>(lroundf(t * 100));
}

inline void speakerGlyph(Canvas &c, float cx, float cy, int waves, Rgb col) {
  c.fillRect(static_cast<int>(cx - 7), static_cast<int>(cy - 4), 5, 8, col);
  const float cone[] = {cx - 3, cy - 4, cx + 3, cy - 9, cx + 3, cy + 9, cx - 3, cy + 4};
  c.polygon(cone, 4, col);
  for (int i = 1; i <= waves; i++) {
    const float x = cx + 4 + i * 5;
    c.capsule(x, cy - 3 - i * 2, x, cy + 3 + i * 2, 1.2f, col);
  }
}


inline void drawSoundStatic(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  c.clear(kBackground);
  drawTitleBar(c, st.screen, i18n::tr(L, Str::Sound));
  speakerGlyph(c, kSliderX - 4, kVolumeSliderY + 34, 0, kMuted);
  speakerGlyph(c, kSliderX + kSliderW - 8, kVolumeSliderY + 34, 2, kText);
  const Rect r = kChimeRow;
  c.roundRect(r.x, r.y, r.w, r.h, 18, rgb(0x14171D));
  c.text(kFontBody, 34, r.y + r.h / 2 + 7, i18n::tr(L, Str::HourlyChime), kText);
  drawSwitch(c, r.x + r.w - 18, r.y + r.h / 2, st.info.chimeOn);
  c.text(kFontSmall, W / 2, 364, i18n::tr(L, Str::ChimeHint), kMuted, Align::Center);
}

inline void drawSoundDynamic(Canvas &c, const UiState &st) {
  const int v = st.info.volumePercent;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d %%", v);
  c.text(kFontTemp, W / 2, kValueBaseline, buf, v > 0 ? kText : kMuted, Align::Center);
  const float kx = kSliderX + v / 100.0f * kSliderW;
  c.capsule(kSliderX, kVolumeSliderY, kSliderX + kSliderW, kVolumeSliderY, 5, rgb(0x2A2F38));
  c.capsule(kSliderX, kVolumeSliderY, kx, kVolumeSliderY, 5, kAccent);
  c.fillCircle(kx, kVolumeSliderY, 15, kText);
}

} // namespace ui
