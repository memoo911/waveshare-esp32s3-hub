#pragma once

// Settings: the scrolling list of rows that lead to the other settings pages.

#include "ui/Widgets.h"

namespace ui {

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
// The rows scroll under a fixed title bar. They are drawn once into a tall
// "list" canvas (kSettingsContentH rows, same coordinates as the screen at
// scroll 0) and the visible window is copied into the frame while scrolling.
constexpr int kListTop = 64; // first screen row of the scrolling area (even)
constexpr int kRowsTop = 74;
constexpr int kRowH = 56;
constexpr int kRowGap = 8;
constexpr int kRowCount = 7;
constexpr int kWifiRow = 0;
constexpr int kLocationRow = 1;
constexpr int kBrightnessRow = 2;
constexpr int kSoundRow = 3;
constexpr int kAppsRow = 4;
constexpr int kLanguageRow = 5;
constexpr int kTurnOffRow = 6;
constexpr int kSettingsFooter = kRowsTop + kRowCount * (kRowH + kRowGap);
constexpr int kSettingsContentH = kSettingsFooter + 64;
constexpr int kSettingsMaxScroll = kSettingsContentH - screen::H;

// Row position in list coordinates (= screen coordinates at scroll 0).
inline Rect rowRect(int i) { return Rect{14, kRowsTop + i * (kRowH + kRowGap), 340, kRowH}; }

inline void drawRow(Canvas &c, int i, const char *label, const char *value,
                    bool opens, Rgb labelColor = kText, Rgb valueColor = kMuted) {
  const Rect r = rowRect(i);
  c.roundRect(r.x, r.y, r.w, r.h, 18, rgb(0x14171D));
  const int mid = r.y + kRowH / 2;
  c.text(kFontBody, 34, mid + 7, label, labelColor);
  const int right = opens ? 316 : 334;
  c.text(kFontSmall, right, mid + 6, value, valueColor, Align::Right);
  if (opens) {
    // "›" chevron: this row opens a page.
    c.capsule(326, mid - 7, 332, mid, 1.4f, kMuted);
    c.capsule(332, mid, 326, mid + 7, 1.4f, kMuted);
  }
}

// Fixed part: background and title bar.
inline void drawSettingsStatic(Canvas &c, const UiState &st) {
  c.clear(kBackground);
  drawTitleBar(c, st.screen, i18n::tr(st.screen.lang, Str::Settings));
}

// The scrolling rows, into a W x kSettingsContentH canvas.
inline void drawSettingsList(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  const DeviceInfo &d = st.info;
  const Lang L = s.lang;
  c.clear(kBackground);

  char buf[48];
  drawRow(c, kWifiRow, i18n::tr(L, Str::Wifi),
          s.wifiConnected && d.ssid[0] ? d.ssid : i18n::tr(L, Str::NotConnected), true);
  drawRow(c, kLocationRow, i18n::tr(L, Str::Location), s.city, true);
  snprintf(buf, sizeof(buf), "%d %%", d.brightnessPercent);
  drawRow(c, kBrightnessRow, i18n::tr(L, Str::Brightness), buf, true);
  snprintf(buf, sizeof(buf), "%d %%", d.volumePercent);
  drawRow(c, kSoundRow, i18n::tr(L, Str::Sound), buf, true);
  // Not there yet: apps for the home screen.
  drawRow(c, kAppsRow, i18n::tr(L, Str::Apps), appName(L, st.app), true);
  drawRow(c, kLanguageRow, i18n::tr(L, Str::Language), i18n::tr(L, Str::LanguageName), false);
  drawRow(c, kTurnOffRow, i18n::tr(L, Str::TurnOff), "", true, kDanger);

  c.text(kFontSmall, W / 2, kSettingsFooter + 18, i18n::tr(L, Str::SoundSoon), kMuted,
         Align::Center);
  if (d.ip[0]) {
    snprintf(buf, sizeof(buf), "IP %s", d.ip);
    c.text(kFontSmall, W / 2, kSettingsFooter + 42, buf, kFaint, Align::Center);
  }
}

// Copy the visible part of the list into the frame, plus a thin scroll bar.
inline void composeSettings(Canvas &frame, const uint16_t *list, int scroll) {
  const int w = frame.width();
  for (int y = kListTop; y < screen::H; y++) {
    memcpy(frame.pixels() + y * w, list + (y + scroll) * w, w * sizeof(uint16_t));
  }
  const float top = kListTop + 6, bottom = screen::H - 8;
  const float len = (bottom - top) * screen::H / kSettingsContentH;
  const float y0 = top + (bottom - top - len) * scroll / kSettingsMaxScroll;
  frame.capsule(w - 5, y0, w - 5, y0 + len, 1.6f, kFaint);
}

// A tap at screen (x, y) in list coordinates, or -1 on the title bar.
inline int listY(const UiState &st, int y) { return y < kListTop ? -100000 : y + st.scroll; }

} // namespace ui
