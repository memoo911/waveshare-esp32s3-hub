#pragma once

// AI Assistant: home tile (robot) and its page (ready, listening, thinking,
// answer, error).

#include "apps/assistant/RobotIcon.h"
#include "ui/Widgets.h"

namespace ui {

constexpr Rgb kAiAccent = rgb(0x9B8CFF);
constexpr Rgb kAiDim = rgb(0x2A2550);
constexpr Rgb kAiTileTop = rgb(0x2C2660), kAiTileBottom = rgb(0x15122E);

inline void drawAssistantTile(Canvas &c, const UiState &st) {
  const Rect &wt = kWeatherTile;
  c.roundRectGradient(wt.x, wt.y, wt.w, wt.h, kTileRadius, kAiTileTop, kAiTileBottom);
  // Robot picture with its own opacity.
  const int x0 = wt.x + (wt.w - kRobotW) / 2, y0 = wt.y + (wt.h - kRobotH) / 2;
  for (int y = 0; y < kRobotH; y++) {
    for (int x = 0; x < kRobotW; x++) {
      const int i = y * kRobotW + x;
      const int a = kRobotAlpha[i];
      if (!a) continue;
      const int px = x0 + x, py = y0 + y;
      if (px < 0 || py < 0 || px >= c.width() || py >= c.height()) continue;
      Canvas::blend565(c.pixels()[py * c.width() + px], kRobotRgb[i], a == 255 ? 256 : a);
    }
  }
  c.text(kFontBody, wt.x + wt.w / 2, kTileLabelBaseline, i18n::tr(st.screen.lang, Str::AssistantApp),
         kText, Align::Center);
}


// ---------------------------------------------------------------------------
// Assistant: hold BOOT to ask, the answer is shown as text
// ---------------------------------------------------------------------------
constexpr float kMicCx = 184, kMicCy = 190;
constexpr Rect kNewChatButton{24, 382, 320, 54};
constexpr Rect kMeterRect{70, 280, 228, 66};
constexpr Rect kRecTimeRect{0, 384, W, 26};
constexpr Rect kDotsRect{110, 176, 148, 50};
constexpr int kAnswerTop = 112;    // first baseline
constexpr int kAnswerBottom = 344; // last baseline

inline void micGlyph(Canvas &c, float cx, float cy, float s, Rgb col) {
  c.capsule(cx, cy - s * 0.55f, cx, cy + s * 0.05f, s * 0.28f, col);
  for (int k = 0; k < 16; k++) {
    const float a0 = 3.14159265f * k / 16, a1 = 3.14159265f * (k + 1) / 16;
    c.capsule(cx + cosf(a0) * s * 0.48f, cy + sinf(a0) * s * 0.48f - s * 0.15f,
              cx + cosf(a1) * s * 0.48f, cy + sinf(a1) * s * 0.48f - s * 0.15f, s * 0.06f, col);
  }
  c.capsule(cx, cy + s * 0.33f, cx, cy + s * 0.6f, s * 0.06f, col);
  c.capsule(cx - s * 0.22f, cy + s * 0.62f, cx + s * 0.22f, cy + s * 0.62f, s * 0.06f, col);
}

inline void drawNewChatButton(Canvas &c, Lang L, bool enabled) {
  const Rect &b = kNewChatButton;
  c.roundRect(b.x, b.y, b.w, b.h, b.h / 2.0f, enabled ? rgb(0x1C1F2B) : rgb(0x111318));
  const char *label = i18n::tr(L, Str::AsNewChat);
  const int tw = c.textWidth(kFontBody, label);
  const float px = W / 2.0f - (tw + 26) / 2.0f + 8, py = b.y + b.h / 2.0f;
  const Rgb plus = enabled ? kAiAccent : kFaint;
  c.capsule(px - 8, py, px + 8, py, 1.6f, plus);
  c.capsule(px, py - 8, px, py + 8, 1.6f, plus);
  c.text(kFontBody, static_cast<int>(px + 18), b.y + 35, label, enabled ? kText : kFaint);
}

// Word wrap on spaces (UTF-8 safe). Draws when `draw` is set; returns the
// number of lines. Stops after maxLines, ending the last one with "…".
inline int wrapText(Canvas &c, const Font &f, int x, int y, int maxW, int lineH, const char *text,
                    Rgb col, int maxLines, bool draw, Align align = Align::Left) {
  char line[256];
  int lines = 0;
  const char *p = text;
  while (*p && lines < maxLines) {
    while (*p == ' ' || *p == '\n') p++;
    if (!*p) break;
    // Take words while they fit.
    const char *end = p, *lastFit = nullptr;
    for (;;) {
      const char *q = end;
      while (*q && *q != ' ' && *q != '\n') q++;
      const size_t n = static_cast<size_t>(q - p);
      if (n >= sizeof(line) - 4) break;
      memcpy(line, p, n);
      line[n] = 0;
      if (c.textWidth(f, line) > maxW && lastFit) break;
      lastFit = q;
      end = q;
      if (!*q || *q == '\n') break;
      end = q + 1;
    }
    if (!lastFit) lastFit = end; // a single very long word
    size_t n = static_cast<size_t>(lastFit - p);
    if (n >= sizeof(line) - 4) n = sizeof(line) - 4;
    memcpy(line, p, n);
    line[n] = 0;
    p = lastFit;
    lines++;
    if (lines == maxLines && *p) {
      // More text than room: cut the last line with an ellipsis.
      while (n > 0 && c.textWidth(f, (strcpy(line + n, "…"), line)) > maxW) {
        do { n--; } while (n > 0 && (static_cast<uint8_t>(line[n]) & 0xC0) == 0x80);
      }
      strcpy(line + n, "…");
    }
    if (draw) c.text(f, x, y + (lines - 1) * lineH, line, col, align);
  }
  return lines;
}

inline const char *assistantErrorText(const AssistantInfo &a, Lang L, char *buf, size_t n) {
  switch (a.error) {
  case AssistantError::NotSetUp: return i18n::tr(L, Str::AsNotSetUp);
  case AssistantError::NoWifi: return i18n::tr(L, Str::AsNoWifi);
  case AssistantError::Unauthorized: return i18n::tr(L, Str::AsUnauthorized);
  case AssistantError::NotHeard: return i18n::tr(L, Str::AsNotHeard);
  case AssistantError::TooShort: return i18n::tr(L, Str::AsTooShort);
  default:
    snprintf(buf, n, i18n::tr(L, Str::AsServerError), a.status);
    return buf;
  }
}

inline void drawAssistantStatic(Canvas &c, const UiState &st) {
  const Lang L = st.screen.lang;
  const AssistantInfo &a = st.assistant;
  char buf[96];
  c.clear(kBackground);
  drawTitleBar(c, st.screen, i18n::tr(L, Str::AssistantApp));
  switch (a.phase) {
  case AssistantPhase::Ready:
    c.glow(kMicCx, kMicCy, 110, kAiAccent, 0.18f);
    c.fillCircle(kMicCx, kMicCy, 72, kAiDim);
    c.ring(kMicCx, kMicCy, 72, 3, kAiAccent);
    micGlyph(c, kMicCx, kMicCy, 64, kAiAccent);
    c.text(kFontBody, W / 2, 304, i18n::tr(L, Str::AsHold1), kText, Align::Center);
    c.text(kFontBody, W / 2, 334, i18n::tr(L, Str::AsHold2), kText, Align::Center);
    c.text(kFontSmall, W / 2, 362, i18n::tr(L, Str::AsLangHint), kMuted, Align::Center);
    drawNewChatButton(c, L, a.turns > 0);
    break;
  case AssistantPhase::Listening:
    c.glow(kMicCx, kMicCy, 130, kAiAccent, 0.35f);
    c.fillCircle(kMicCx, kMicCy, 80, kAiAccent);
    micGlyph(c, kMicCx, kMicCy, 64, kAiTileBottom);
    c.text(kFontTitle, W / 2, 372, i18n::tr(L, Str::AsListening), kText, Align::Center);
    break;
  case AssistantPhase::Thinking:
    c.text(kFontTitle, W / 2, 272, i18n::tr(L, Str::AsThinking), kText, Align::Center);
    snprintf(buf, sizeof(buf), i18n::tr(L, Str::AsQuestionN), a.turns + 1);
    c.text(kFontSmall, W / 2, 300, buf, kMuted, Align::Center);
    drawNewChatButton(c, L, true);
    break;
  case AssistantPhase::Answer: {
    // Regular size when it fits, otherwise the smaller font.
    const int maxW = W - 48;
    const Font *f = &kFontBody;
    int lineH = 30;
    int maxLines = (kAnswerBottom - kAnswerTop) / lineH + 1;
    if (wrapText(c, kFontBody, 24, kAnswerTop, maxW, lineH, a.answer, kText, 99, false) > maxLines) {
      f = &kFontSmall;
      lineH = 24;
      maxLines = (kAnswerBottom - kAnswerTop) / lineH + 1;
    }
    const int lines = wrapText(c, *f, 24, kAnswerTop, maxW, lineH, a.answer, kText, maxLines, true);
    snprintf(buf, sizeof(buf), i18n::tr(L, Str::AsNext), a.turns);
    const int footY = kAnswerTop + lines * lineH + 8;
    if (footY < kNewChatButton.y - 8) c.text(kFontSmall, 24, footY, buf, kFaint);
    drawNewChatButton(c, L, true);
    break;
  }
  case AssistantPhase::Error: {
    c.fillCircle(kMicCx, 170, 44, rgb(0x3A2420));
    c.capsule(kMicCx, 150, kMicCx, 176, 4.5f, rgb(0xFF8A70));
    c.fillCircle(kMicCx, 192, 5, rgb(0xFF8A70));
    const char *msg = assistantErrorText(a, L, buf, sizeof(buf));
    c.text(kFontBody, W / 2, 262, msg, kText, Align::Center);
    if (a.error == AssistantError::NotSetUp) {
      wrapText(c, kFontSmall, W / 2, 296, W - 60, 22, i18n::tr(L, Str::AsNotSetUpHint), kMuted, 4,
               true, Align::Center);
    } else {
      c.text(kFontSmall, W / 2, 292, i18n::tr(L, Str::AsTryAgain), kMuted, Align::Center);
    }
    if (a.error != AssistantError::NotSetUp) drawNewChatButton(c, L, a.turns > 0);
    break;
  }
  }
}

inline void drawAssistantDynamic(Canvas &c, const UiState &st) {
  const AssistantInfo &a = st.assistant;
  if (a.phase == AssistantPhase::Listening) {
    // Level bars: louder = taller, with a little wave so it looks alive.
    const float t = st.screen.animTime;
    for (int i = 0; i < 11; i++) {
      const float wave = 0.55f + 0.45f * sinf(t * 9.0f + i * 0.9f);
      const float h = 8 + a.level * wave * 50;
      const float x = kMeterRect.x + 14 + i * 20;
      c.capsule(x, 312 - h / 2, x, 312 + h / 2, 4, kAiAccent);
    }
    char buf[80];
    const int s = a.recordMs / 1000;
    snprintf(buf, sizeof(buf), i18n::tr(st.screen.lang, Str::AsRelease), s / 60, s % 60);
    c.text(kFontSmall, W / 2, 402, buf, kMuted, Align::Center);
  } else if (a.phase == AssistantPhase::Thinking) {
    const float t = st.screen.animTime;
    for (int i = 0; i < 3; i++) {
      const float k = 0.5f + 0.5f * sinf(t * 5.0f - i * 1.1f);
      c.fillCircle(kDotsRect.x + 38 + i * 36, kDotsRect.y + 24, 10 + 4 * k, kAiAccent, 0.4f + 0.6f * k);
    }
  }
}

// The "New conversation" button (only where it's drawn and active).
inline bool tapsNewChat(const UiState &st, int x, int y) {
  if (st.page != Page::Assistant) return false;
  const AssistantInfo &a = st.assistant;
  const bool shown = a.phase == AssistantPhase::Thinking || a.phase == AssistantPhase::Answer ||
                     (a.phase == AssistantPhase::Ready && a.turns > 0) ||
                     (a.phase == AssistantPhase::Error && a.error != AssistantError::NotSetUp &&
                      a.turns > 0);
  return shown && inside(kNewChatButton, x, y, 6);
}

} // namespace ui
