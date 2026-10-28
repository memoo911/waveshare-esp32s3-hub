#pragma once

// Spirit level: home tile and its page (round level when flat, tube level on
// an edge).

#include "ui/Widgets.h"

namespace ui {

constexpr Rgb kLevelGlass = rgb(0x0F2A2A);
constexpr Rgb kBubble = rgb(0xE6F7F1);

inline void drawLevelTile(Canvas &c, const UiState &st) {
  const Rect &wt = kWeatherTile;
  c.roundRectGradient(wt.x, wt.y, wt.w, wt.h, kTileRadius, rgb(0x163A36), rgb(0x0C1C1B));
  const float cx = wt.x + wt.w / 2.0f, cy = wt.y + 66;
  c.fillCircle(cx, cy, 38, kLevelGlass);
  c.ring(cx, cy, 38, 3, kAccent);
  c.ring(cx, cy, 14, 1.5f, kAccent, 0.5f);
  c.glow(cx, cy, 20, kAccent, 0.35f);
  c.fillCircle(cx, cy, 11, kBubble);
  c.fillCircle(cx - 3.5f, cy - 3.5f, 3.5f, rgb(0xFFFFFF));
  c.text(kFontBody, wt.x + wt.w / 2, kTileLabelBaseline, i18n::tr(st.screen.lang, Str::LevelTile),
         kText, Align::Center);
}


// ---------------------------------------------------------------------------
// Spirit level
// ---------------------------------------------------------------------------
// Flat: round "bull's eye" level. Edge: tube level along the bottom (or side).
constexpr float kLevelCx = 184, kLevelCy = 214, kLevelR = 128;
constexpr float kBubbleR = 26;
constexpr float kLevelFullScaleDeg = 12;   // bubble reaches the rim at this tilt
constexpr float kLevelOkDeg = 0.5f;       // counts as level
constexpr Rect kLevelDialRect{40, 70, 288, 290};
constexpr Rect kLevelReadoutRect{0, 352, W, 40};
constexpr Rect kZeroButton{94, 396, 180, 46};
// Tube (edge mode).
constexpr Rect kTubeH{34, 178, 300, 72};
constexpr Rect kTubeV{148, 76, 72, 276};
constexpr float kTubeTravel = 104;        // bubble travel from the centre
constexpr float kTubeBubbleLen = 66, kTubeBubbleThick = 40;

inline bool levelOk(const LevelInfo &l) {
  return l.edge ? fabsf(l.angle) < kLevelOkDeg
                : fabsf(l.x) < kLevelOkDeg && fabsf(l.y) < kLevelOkDeg;
}

inline void formatDeg(char *out, size_t n, float deg) {
  float v = roundf(fabsf(deg) * 10) / 10;
  snprintf(out, n, "%.1f°", v);
}

inline void drawLevelStatic(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  const LevelInfo &l = st.level;
  c.clear(kBackground);
  drawTitleBar(c, st.screen, i18n::tr(L, Str::LevelTile));
  if (!l.sensor) {
    c.text(kFontBody, W / 2, 230, i18n::tr(L, Str::NoSensor), kMuted, Align::Center);
    return;
  }
  if (!l.edge) {
    // Glass dial with rings at 2°, 5° and 10° and a faint crosshair.
    c.glow(kLevelCx, kLevelCy, kLevelR + 24, kAccent, 0.10f);
    c.fillCircle(kLevelCx, kLevelCy, kLevelR, kLevelGlass);
    const float degs[] = {2, 5, 10};
    for (float d : degs) {
      const float r = kLevelR * d / kLevelFullScaleDeg;
      c.ring(kLevelCx, kLevelCy, r, 1.2f, rgb(0x2E5A55));
      if (d >= 5) {
        char lbl[8];
        snprintf(lbl, sizeof(lbl), "%d°", static_cast<int>(d));
        c.text(kFontSmall, static_cast<int>(kLevelCx + r * 0.7071f) + 4,
               static_cast<int>(kLevelCy - r * 0.7071f) - 2, lbl, rgb(0x4F8A82));
      }
    }
    c.capsule(kLevelCx - kLevelR + 10, kLevelCy, kLevelCx + kLevelR - 10, kLevelCy, 0.7f,
              rgb(0x24433F));
    c.capsule(kLevelCx, kLevelCy - kLevelR + 10, kLevelCx, kLevelCy + kLevelR - 10, 0.7f,
              rgb(0x24433F));
    c.ring(kLevelCx, kLevelCy, kLevelR, 3, rgb(0x2E5A55));
  } else {
    const Rect &t = l.vertical ? kTubeV : kTubeH;
    c.roundRect(t.x, t.y, t.w, t.h, (l.vertical ? t.w : t.h) / 2.0f, kLevelGlass);
    // Marks either side of the centre: the bubble fits between them when level.
    const float cx = t.x + t.w / 2.0f, cy = t.y + t.h / 2.0f;
    const float gap = kTubeBubbleLen / 2 + 8;
    if (l.vertical) {
      c.capsule(t.x + 12, cy - gap, t.x + t.w - 12, cy - gap, 1.2f, rgb(0x2E5A55));
      c.capsule(t.x + 12, cy + gap, t.x + t.w - 12, cy + gap, 1.2f, rgb(0x2E5A55));
    } else {
      c.capsule(cx - gap, t.y + 12, cx - gap, t.y + t.h - 12, 1.2f, rgb(0x2E5A55));
      c.capsule(cx + gap, t.y + 12, cx + gap, t.y + t.h - 12, 1.2f, rgb(0x2E5A55));
    }
  }
  pill(c, kZeroButton, rgb(0x14171D));
  pillLabel(c, kZeroButton, kFontBody, i18n::tr(L, Str::SetZero),
            l.calibrated ? kAccent : kText);
}

inline void drawBubble(Canvas &c, float x, float y, bool ok) {
  const Rgb col = ok ? kAccent : kBubble;
  c.glow(x, y, kBubbleR + 14, col, ok ? 0.45f : 0.25f);
  c.fillCircle(x, y, kBubbleR, col, 0.92f);
  c.ring(x, y, kBubbleR, 2, rgb(0xFFFFFF), 0.5f);
  c.fillCircle(x - kBubbleR * 0.35f, y - kBubbleR * 0.35f, kBubbleR * 0.28f, rgb(0xFFFFFF), 0.85f);
}

// Elongated bubble, as in a real tube level.
inline void drawTubeBubble(Canvas &c, float x, float y, bool vertical, bool ok) {
  const Rgb col = ok ? kAccent : kBubble;
  const float h = (kTubeBubbleLen - kTubeBubbleThick) / 2, r = kTubeBubbleThick / 2;
  const float x0 = vertical ? x : x - h, x1 = vertical ? x : x + h;
  const float y0 = vertical ? y - h : y, y1 = vertical ? y + h : y;
  c.glow(x, y, kTubeBubbleLen / 2 + 14, col, ok ? 0.40f : 0.22f);
  c.capsule(x0, y0, x1, y1, r, col, 0.92f);
  if (vertical) c.capsule(x - r * 0.4f, y0, x - r * 0.4f, y1 - h * 0.6f, r * 0.2f, rgb(0xFFFFFF), 0.8f);
  else c.capsule(x0, y - r * 0.4f, x1 - h * 0.6f, y - r * 0.4f, r * 0.2f, rgb(0xFFFFFF), 0.8f);
}

inline void drawLevelDynamic(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  const LevelInfo &l = st.level;
  if (!l.sensor) return;
  const bool ok = levelOk(l);
  char buf[32];
  if (!l.edge) {
    // The bubble floats to the high side: opposite to the tilt.
    float dx = -l.x / kLevelFullScaleDeg, dy = -l.y / kLevelFullScaleDeg;
    const float len = sqrtf(dx * dx + dy * dy);
    const float maxLen = (kLevelR - kBubbleR - 4) / kLevelR;
    if (len > maxLen) { dx *= maxLen / len; dy *= maxLen / len; }
    if (ok) c.ring(kLevelCx, kLevelCy, kBubbleR + 6, 3, kAccent);
    drawBubble(c, kLevelCx + dx * kLevelR, kLevelCy + dy * kLevelR, ok);
    const float total = sqrtf(l.x * l.x + l.y * l.y);
    if (ok) snprintf(buf, sizeof(buf), "%s", i18n::tr(L, Str::LevelOk));
    else formatDeg(buf, sizeof(buf), total);
    c.text(kFontTitle, W / 2, 382, buf, ok ? kAccent : kText, Align::Center);
    char xs[16], ys[16];
    formatDeg(xs, sizeof(xs), l.x);
    formatDeg(ys, sizeof(ys), l.y);
    snprintf(buf, sizeof(buf), "X %s", xs);
    c.text(kFontSmall, 24, 382, buf, kMuted);
    snprintf(buf, sizeof(buf), "Y %s", ys);
    c.text(kFontSmall, W - 24, 382, buf, kMuted, Align::Right);
  } else {
    const Rect &t = l.vertical ? kTubeV : kTubeH;
    const float cx = t.x + t.w / 2.0f, cy = t.y + t.h / 2.0f;
    float off = -l.angle / kLevelFullScaleDeg * kTubeTravel;
    off = fmaxf(-kTubeTravel, fminf(kTubeTravel, off));
    if (l.vertical) drawTubeBubble(c, cx, cy + off, true, ok);
    else drawTubeBubble(c, cx + off, cy, false, ok);
    if (ok) snprintf(buf, sizeof(buf), "%s", i18n::tr(L, Str::LevelOk));
    else formatDeg(buf, sizeof(buf), l.angle);
    c.text(kFontTitle, W / 2, 382, buf, ok ? kAccent : kText, Align::Center);
  }
}

inline bool tapsZero(const UiState &st, int x, int y) {
  return st.page == Page::Level && st.level.sensor && inside(kZeroButton, x, y, 10);
}

} // namespace ui
