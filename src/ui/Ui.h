#pragma once

// Pages of the device UI: Home (launcher), Weather (now), Forecast (3 days),
// Settings, Display (brightness, screen-off time), Sound, Turn off, WiFi, WiFi setup (QR to the board's own network) and
// Location (QR to the board's web page), plus the home-screen apps (kitchen
// timer, spirit level, assistant).
//
// Pure drawing and hit testing, no Arduino dependencies, so the PC preview
// (tools/preview.cpp) renders exactly what the device shows. Each page draws
// a static layer (cached per page) and a dynamic layer (redrawn every frame
// inside the rectangles returned by dynamicRects()).
//
// The pages and apps live in their own headers (pages/, apps/); this file
// ties them together: drawing dispatch, layer keys and input.

#include "pages/AppsPage.h"
#include "pages/DisplayPage.h"
#include "pages/HomePage.h"
#include "pages/PowerOffPage.h"
#include "pages/SettingsPage.h"
#include "pages/SoundPage.h"
#include "pages/WifiPages.h"

namespace ui {

// ---------------------------------------------------------------------------
// Page dispatch
// ---------------------------------------------------------------------------
inline void drawStatic(Canvas &c, const UiState &st) {
  switch (st.page) {
  case Page::Home: drawHomeStatic(c, st); break;
  case Page::Weather: screen::drawStatic(c, st.screen); break;
  case Page::Forecast: screen::drawForecastStatic(c, st.screen); break;
  case Page::Settings: drawSettingsStatic(c, st); break;
  case Page::Brightness: drawBrightnessStatic(c, st); break;
  case Page::Wifi: drawWifiStatic(c, st); break;
  case Page::WifiSetup: drawWifiSetupStatic(c, st); break;
  case Page::Location: drawLocationStatic(c, st); break;
  case Page::Sound: drawSoundStatic(c, st); break;
  case Page::PowerOff: drawPowerOffStatic(c, st); break;
  case Page::Apps: drawAppsStatic(c, st); break;
  case Page::Timer: drawTimerStatic(c, st); break;
  case Page::Level: drawLevelStatic(c, st); break;
  case Page::Assistant: drawAssistantStatic(c, st); break;
  }
}

// Rectangles the dynamic layer draws into; restored from the base layer
// before drawDynamic(). Returns the count (at most 4).
inline int dynamicRects(const Canvas &c, const UiState &st, Rect out[4]) {
  switch (st.page) {
  case Page::Home:
    out[0] = screen::colonRect(c);
    out[1] = kTileIconRect;
    return 2;
  case Page::Weather:
    out[0] = screen::colonRect(c);
    out[1] = screen::iconRect();
    return 2;
  case Page::Forecast:
    out[0] = screen::colonRect(c);
    for (int i = 0; i < 3; i++) out[1 + i] = screen::forecastIconRect(i);
    return 4;
  case Page::Settings:
    return 0;
  case Page::Brightness:
    out[0] = kBrightnessDynamic;
    return 1;
  case Page::Sound:
    out[0] = kVolumeDynamic;
    return 1;
  case Page::Timer:
    if (st.timer.state == TimerState::Running || st.timer.state == TimerState::Paused) {
      out[0] = kRingRect;
      return 1;
    }
    return 0;
  case Page::Level:
    if (!st.level.sensor) return 0;
    out[0] = st.level.edge ? (st.level.vertical ? Rect{120, 66, 128, 290}
                                                : Rect{0, 150, W, 130})
                           : kLevelDialRect;
    out[1] = kLevelReadoutRect;
    return 2;
  case Page::Assistant:
    if (st.assistant.phase == AssistantPhase::Listening) {
      out[0] = kMeterRect;
      out[1] = kRecTimeRect;
      return 2;
    }
    if (st.assistant.phase == AssistantPhase::Thinking) {
      out[0] = kDotsRect;
      return 1;
    }
    return 0;
  default:
    return 0;
  }
}

inline void drawDynamic(Canvas &c, const UiState &st) {
  switch (st.page) {
  case Page::Home: drawHomeDynamic(c, st); break;
  case Page::Weather: screen::drawDynamic(c, st.screen); break;
  case Page::Forecast: screen::drawForecastDynamic(c, st.screen); break;
  case Page::Brightness: drawBrightnessDynamic(c, st); break;
  case Page::Sound: drawSoundDynamic(c, st); break;
  case Page::Timer:
    if (st.timer.state == TimerState::Running || st.timer.state == TimerState::Paused) {
      drawTimerRing(c, st);
    }
    break;
  case Page::Level: drawLevelDynamic(c, st); break;
  case Page::Assistant: drawAssistantDynamic(c, st); break;
  default: break;
  }
}

inline void draw(Canvas &c, const UiState &st) {
  drawStatic(c, st);
  drawDynamic(c, st);
}

// FNV-1a over everything the static layer depends on. When it changes, the
// page's base layer is redrawn.
struct Hasher {
  uint32_t h = 2166136261u;
  void bytes(const void *p, size_t n) {
    const uint8_t *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; i++) {
      h ^= b[i];
      h *= 16777619u;
    }
  }
  template <typename T> void add(const T &v) { bytes(&v, sizeof(v)); }
  void str(const char *s) {
    if (s) bytes(s, strlen(s));
    add(static_cast<uint8_t>(0));
  }
};

inline uint32_t staticKey(const UiState &st) {
  const ScreenState &s = st.screen;
  const WeatherData &w = s.weather;
  Hasher h;
  h.add(st.page);
  h.add(s.batteryPercent); h.add(s.charging);
  if (st.page == Page::Brightness) {
    h.add(s.lang); h.add(st.info.screenOff); h.add(st.info.nightDim);
    return h.h;
  }
  if (st.page == Page::Sound) {
    h.add(s.lang); h.add(st.info.chimeOn);
    return h.h;
  }
  h.add(s.timeValid);
  h.add(s.hour); h.add(s.minute); h.add(s.weekday); h.add(s.day); h.add(s.month);
  h.add(w.valid); h.add(w.temperature); h.add(w.feelsLike); h.add(w.humidity);
  h.add(w.windKmh); h.add(w.code); h.add(w.isDay); h.add(w.todayMin);
  h.add(w.todayMax); h.str(w.sunrise); h.str(w.sunset);
  h.add(w.dayCount);
  for (int i = 0; i < w.dayCount && i < 3; i++) {
    h.add(w.days[i].code); h.add(w.days[i].tempMax); h.add(w.days[i].tempMin);
    h.add(w.days[i].precipChance); h.add(w.days[i].day);
  }
  h.add(s.weatherStale); h.str(s.message); h.str(s.city);
  h.add(s.wifiConnected); h.add(s.showBack); h.str(s.toast); h.add(s.lang);
  const DeviceInfo &d = st.info;
  const TimerInfo &t = st.timer;
  switch (st.page) {
  case Page::Home:
    h.add(st.app);
    if (st.app == HomeApp::Timer) { h.add(t.state); h.add(t.minutes); h.add(t.remainingSec); }
    break;
  case Page::Apps:
    h.add(st.app);
    break;
  case Page::Timer:
    h.add(t.state); h.add(t.minutes);
    break;
  case Page::Level:
    h.add(st.level.sensor); h.add(st.level.edge); h.add(st.level.vertical);
    h.add(st.level.calibrated);
    break;
  case Page::Assistant:
    h.add(st.assistant.phase); h.add(st.assistant.error); h.add(st.assistant.status);
    h.add(st.assistant.turns); h.str(st.assistant.answer);
    break;
  case Page::Settings:
    h.str(d.ssid); h.str(d.ip); h.add(d.brightnessPercent); h.add(d.volumePercent);
    h.add(st.app);
    break;
  case Page::Wifi:
    h.str(d.ssid); h.add(d.rssi / 5); h.str(d.ip); h.add(d.pendingDelete);
    h.add(d.netCount);
    for (int i = 0; i < d.netCount; i++) { h.str(d.nets[i].ssid); h.add(d.nets[i].fromCode); }
    break;
  case Page::WifiSetup:
    h.add(d.setupState); h.str(d.apSsid); h.str(d.apPassword); h.str(d.savedSsid);
    h.add(d.setupOverLan); h.str(d.ip); h.str(d.ssid); h.add(d.apClients > 0);
    break;
  case Page::Location:
    h.str(d.ip);
    break;
  default:
    break;
  }
  return h.h;
}

// Changes whenever the dynamic layer would draw something different. When it
// is unchanged the dynamic layer is skipped entirely (no redraw, no flush).
// `frameNo` advances every frame and is mixed in only while something moves.
inline uint32_t dynamicKey(const UiState &st, uint32_t frameNo) {
  const ScreenState &s = st.screen;
  const WeatherData &w = s.weather;
  Hasher h;
  h.add(st.page);
  switch (st.page) {
  case Page::Brightness:
    h.add(st.info.brightnessPercent);
    return h.h;
  case Page::Sound:
    h.add(st.info.volumePercent);
    return h.h;
  case Page::Timer:
    h.add(st.timer.state); h.add(st.timer.remainingSec);
    return h.h;
  case Page::Level: {
    // Bubble position to the pixel, readout to 0.1°.
    const LevelInfo &l = st.level;
    h.add(static_cast<int>(roundf(l.x * 20))); h.add(static_cast<int>(roundf(l.y * 20)));
    h.add(static_cast<int>(roundf(l.angle * 20)));
    return h.h;
  }
  case Page::Assistant:
    h.add(st.assistant.phase);
    if (st.assistant.phase == AssistantPhase::Listening ||
        st.assistant.phase == AssistantPhase::Thinking) {
      h.add(frameNo);
    }
    return h.h;
  case Page::Home:
  case Page::Weather: {
    h.add(s.second % 2); h.add(s.timeValid);
    if (w.valid && icons::isAnimated(w.code)) h.add(frameNo);
    return h.h;
  }
  case Page::Forecast: {
    h.add(s.second % 2); h.add(s.timeValid);
    bool moving = false;
    for (int i = 0; i < w.dayCount && i < 3; i++) moving |= icons::isAnimated(w.days[i].code);
    if (w.valid && moving) h.add(frameNo);
    return h.h;
  }
  default:
    return h.h;
  }
}

// ---------------------------------------------------------------------------
// Input: each handler returns the page to show (the current one = no change)
// ---------------------------------------------------------------------------
constexpr Rect kBackArea{0, 0, 130, 70};

// Where swipe right leads from each page.
inline Page parentOf(Page p) {
  switch (p) {
  case Page::Forecast: return Page::Weather;
  case Page::Brightness:
  case Page::Sound:
  case Page::PowerOff:
  case Page::Wifi:
  case Page::Location:
  case Page::Apps: return Page::Settings;
  case Page::WifiSetup: return Page::Wifi;
  default: return Page::Home;
  }
}

inline Page onTap(const UiState &st, int x, int y) {
  const Page p = st.page;
  if (p != Page::Home && inside(kBackArea, x, y)) {
    // The ‹ in the top bar: weather pages go home, settings pages go up.
    return p == Page::Forecast ? Page::Home : parentOf(p);
  }
  switch (p) {
  case Page::Home:
    if (inside(kWeatherTile, x, y, kTouchMargin)) {
      return st.app == HomeApp::Timer ? Page::Timer
             : st.app == HomeApp::Level ? Page::Level
             : st.app == HomeApp::Assistant ? Page::Assistant
                                        : Page::Weather;
    }
    if (inside(kSettingsTile, x, y, kTouchMargin)) return Page::Settings;
    break;
  case Page::Settings:
    y = listY(st, y);
    if (inside(rowRect(kWifiRow), x, y, kRowGap / 2)) return Page::Wifi;
    if (inside(rowRect(kLocationRow), x, y, kRowGap / 2)) return Page::Location;
    if (inside(rowRect(kBrightnessRow), x, y, kRowGap / 2)) return Page::Brightness;
    if (inside(rowRect(kSoundRow), x, y, kRowGap / 2)) return Page::Sound;
    if (inside(rowRect(kAppsRow), x, y, kRowGap / 2)) return Page::Apps;
    // Last row: the footer under it counts too.
    if (inside(Rect{14, rowRect(kTurnOffRow).y, 340, kSettingsContentH}, x, y, kRowGap / 2)) {
      return Page::PowerOff;
    }
    break;
  case Page::Wifi:
    if (inside(kAddNetworkButton, x, y, kTouchMargin)) return Page::WifiSetup;
    break;
  case Page::PowerOff:
    if (inside(kCancelButton, x, y, kTouchMargin)) return Page::Settings;
    break;
  default:
    break;
  }
  return p;
}

// Tapping the Language row switches between English and Croatian.
inline bool tapsLanguage(const UiState &st, int x, int y) {
  return st.page == Page::Settings && inside(rowRect(kLanguageRow), x, listY(st, y), kRowGap / 2);
}

// The red button on the Turn off page.
inline bool tapsPowerOff(const UiState &st, int x, int y) {
  return st.page == Page::PowerOff && inside(kPowerOffButton, x, y, 6);
}

inline Page onSwipeRight(const UiState &st) {
  return st.page == Page::Home ? Page::Home : parentOf(st.page);
}

inline Page onSwipeLeft(const UiState &st) {
  return st.page == Page::Weather ? Page::Forecast : st.page;
}

// Brightness or volume slider: returns the new value if the finger is on the
// slider of the page that is open.
inline bool sliderDrag(const UiState &st, int x, int y, int &value) {
  if (st.page == Page::Brightness && inside(kSliderTouch, x, y)) {
    value = sliderValueAt(x);
    return true;
  }
  if (st.page == Page::Sound && inside(kVolumeTouch, x, y)) {
    value = volumeAt(x);
    return true;
  }
  return false;
}

// Display page: the first row under the slider steps through the screen-off
// times, the second switches night dimming on or off.
inline bool tapsScreenOff(const UiState &st, int x, int y) {
  return st.page == Page::Brightness && x >= 0 && y >= kOptionsTop && y < kOptionsSplit;
}

inline bool tapsNightDim(const UiState &st, int x, int y) {
  return st.page == Page::Brightness && x >= 0 && y >= kOptionsSplit;
}

// Tapping the hourly chime row switches it on or off. Everything below the
// volume slider counts, so the hint text under the row works too.
inline bool tapsChime(const UiState &st, int x, int y) {
  const int top = kVolumeTouch.y + kVolumeTouch.h + 4;
  return st.page == Page::Sound && inside(Rect{0, top, W, screen::H - top}, x, y);
}

} // namespace ui
