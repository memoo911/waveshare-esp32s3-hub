#pragma once

// Kitchen timer: home tile and its page (set, count down, ring).

#include "ui/Widgets.h"

namespace ui {

// "m:ss" for the timer.
inline void formatMinSec(char *out, size_t n, int sec) {
  if (sec < 0) sec = 0;
  snprintf(out, n, "%d:%02d", sec / 60, sec % 60);
}

constexpr Rgb kTimerColor = rgb(0xF2B84B);

// Stopwatch icon: ring, crown button and a hand.
inline void stopwatchGlyph(Canvas &c, float cx, float cy, float r, Rgb col) {
  c.ring(cx, cy, r, r * 0.16f, col);
  c.capsule(cx - r * 0.22f, cy - r * 1.32f, cx + r * 0.22f, cy - r * 1.32f, r * 0.11f, col);
  c.capsule(cx, cy - r * 1.3f, cx, cy - r * 1.05f, r * 0.09f, col);
  c.capsule(cx, cy, cx, cy - r * 0.62f, r * 0.09f, col);
  c.fillCircle(cx, cy, r * 0.12f, col);
}

inline void drawTimerTile(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  const TimerInfo &t = st.timer;
  const Rect &wt = kWeatherTile;
  c.roundRectGradient(wt.x, wt.y, wt.w, wt.h, kTileRadius, rgb(0x3A2E17), rgb(0x1C170F));
  stopwatchGlyph(c, wt.x + wt.w / 2, wt.y + 68, 32, kTimerColor);
  char buf[16];
  if (t.state == TimerState::Running || t.state == TimerState::Paused) {
    formatMinSec(buf, sizeof(buf), t.remainingSec);
  } else if (t.state == TimerState::Ringing) {
    snprintf(buf, sizeof(buf), "0:00");
  } else {
    snprintf(buf, sizeof(buf), "%d:00", t.minutes);
  }
  c.text(kFontTitle, wt.x + wt.w / 2, wt.y + wt.h - 18, buf,
         t.state == TimerState::Idle ? kMuted : kText, Align::Center);
  c.text(kFontBody, wt.x + wt.w / 2, kTileLabelBaseline, i18n::tr(s.lang, Str::TimerTile),
         kText, Align::Center);
}


// ---------------------------------------------------------------------------
// Kitchen timer
// ---------------------------------------------------------------------------
enum class TimerAction : uint8_t { None, Minus, Plus, Preset, Start, Pause, Resume, Reset, Stop };
constexpr int kTimerPresets[] = {3, 5, 8, 10};
constexpr int kTimerPresetCount = 4;
constexpr int kTimerMaxMinutes = 99;
// Idle page.
constexpr int kTimerValueBaseline = 176;
constexpr Rect kMinusButton{24, 204, 152, 64};
constexpr Rect kPlusButton{192, 204, 152, 64};
inline Rect presetRect(int i) { return Rect{24 + i * 82, 284, 74, 54}; }
constexpr Rect kStartButton{24, 358, 320, 70};
// Running / paused page.
constexpr float kRingCx = 184, kRingCy = 200, kRingR = 108;
constexpr Rect kRingRect{0, 66, W, 272};
constexpr Rect kPauseButton{24, 358, 152, 70};
constexpr Rect kResetButton{192, 358, 152, 70};
// Ringing page.
constexpr Rect kStopButton{24, 300, 320, 120};
constexpr Rgb kRingingBg = rgb(0x2A0E0E);

inline void drawTimerIdle(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  const TimerInfo &t = st.timer;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d:00", t.minutes);
  c.text(kFontTemp, W / 2, kTimerValueBaseline, buf, kText, Align::Center);
  pill(c, kMinusButton, rgb(0x14171D));
  pillLabel(c, kMinusButton, kFontTemp, "-", kText, 26);
  pill(c, kPlusButton, rgb(0x14171D));
  pillLabel(c, kPlusButton, kFontTemp, "+", kText, 26);
  for (int i = 0; i < kTimerPresetCount; i++) {
    const Rect r = presetRect(i);
    const bool on = t.minutes == kTimerPresets[i];
    c.roundRect(r.x, r.y, r.w, r.h, r.h / 2.0f, on ? rgb(0x3A2E17) : rgb(0x14171D));
    snprintf(buf, sizeof(buf), "%d min", kTimerPresets[i]);
    pillLabel(c, r, kFontSmall, buf, on ? kTimerColor : kText, 6);
  }
  pill(c, kStartButton, kTimerColor);
  pillLabel(c, kStartButton, kFontTitle, i18n::tr(L, Str::Start), rgb(0x000000), 9);
}

inline void drawTimerButtons(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  const bool paused = st.timer.state == TimerState::Paused;
  pill(c, kPauseButton, rgb(0x14171D));
  pillLabel(c, kPauseButton, kFontBody, i18n::tr(L, paused ? Str::Resume : Str::Pause),
            paused ? kTimerColor : kText);
  pill(c, kResetButton, rgb(0x14171D));
  pillLabel(c, kResetButton, kFontBody, i18n::tr(L, Str::Reset), kDanger);
}

inline void drawTimerRing(Canvas &c, const UiState &st) {
  const TimerInfo &t = st.timer;
  const bool paused = t.state == TimerState::Paused;
  c.ring(kRingCx, kRingCy, kRingR, 14, rgb(0x2A2F38));
  // Remaining part of the ring, clockwise from the top, drawn as short arcs.
  const float frac = t.totalSec > 0 ? clamp01(static_cast<float>(t.remainingSec) / t.totalSec) : 0;
  const int steps = static_cast<int>(frac * 120);
  const Rgb col = paused ? kMuted : kTimerColor;
  for (int i = 0; i < steps; i++) {
    const float a0 = -1.5707963f + i * (6.2831853f / 120);
    const float a1 = a0 + 6.2831853f / 120;
    c.capsule(kRingCx + cosf(a0) * kRingR, kRingCy + sinf(a0) * kRingR,
              kRingCx + cosf(a1) * kRingR, kRingCy + sinf(a1) * kRingR, 7, col);
  }
  char buf[16];
  formatMinSec(buf, sizeof(buf), t.remainingSec);
  c.text(kFontTemp, static_cast<int>(kRingCx), static_cast<int>(kRingCy) + 30, buf,
         paused ? kMuted : kText, Align::Center);
}

inline void drawTimerStatic(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  const TimerInfo &t = st.timer;
  if (t.state == TimerState::Ringing) {
    c.clear(kRingingBg);
    drawTitleBar(c, st.screen, i18n::tr(L, Str::TimerTile));
    stopwatchGlyph(c, W / 2.0f, 120, 26, kTimerColor);
    c.text(kFontTemp, W / 2, 240, "0:00", kText, Align::Center);
    c.text(kFontTitle, W / 2, 280, i18n::tr(L, Str::TimesUp), kTimerColor, Align::Center);
    pill(c, kStopButton, kDanger);
    pillLabel(c, kStopButton, kFontTitle, i18n::tr(L, Str::Stop), rgb(0x000000), 9);
    return;
  }
  c.clear(kBackground);
  drawTitleBar(c, st.screen, i18n::tr(L, Str::TimerTile));
  if (t.state == TimerState::Idle) drawTimerIdle(c, st);
  else drawTimerButtons(c, st);
}

inline TimerAction timerActionAt(const UiState &st, int x, int y, int &preset) {
  if (st.page != Page::Timer) return TimerAction::None;
  const TimerInfo &t = st.timer;
  switch (t.state) {
  case TimerState::Ringing:
    return TimerAction::Stop; // anywhere on the screen
  case TimerState::Idle:
    if (inside(kMinusButton, x, y, 6)) return TimerAction::Minus;
    if (inside(kPlusButton, x, y, 6)) return TimerAction::Plus;
    for (int i = 0; i < kTimerPresetCount; i++) {
      if (inside(presetRect(i), x, y, 4)) {
        preset = kTimerPresets[i];
        return TimerAction::Preset;
      }
    }
    if (inside(kStartButton, x, y, 10)) return TimerAction::Start;
    return TimerAction::None;
  case TimerState::Running:
  case TimerState::Paused:
    if (inside(kPauseButton, x, y, 10)) {
      return t.state == TimerState::Paused ? TimerAction::Resume : TimerAction::Pause;
    }
    if (inside(kResetButton, x, y, 10)) return TimerAction::Reset;
    return TimerAction::None;
  }
  return TimerAction::None;
}

} // namespace ui
