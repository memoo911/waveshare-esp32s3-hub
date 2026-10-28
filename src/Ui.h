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

#include "RobotIcon.h"
#include "Screen.h"
#include "qrcodegen.h"

enum class Page : uint8_t {
  Home,
  Weather,
  Forecast,
  Settings,
  Brightness,
  Wifi,
  WifiSetup,
  Location,
  Sound,
  PowerOff,
  Apps,
  Timer,
  Level,
  Assistant
};
constexpr int kPageCount = 14;

// The app shown on the home screen next to Settings (Settings -> Apps).
enum class HomeApp : uint8_t { Weather = 0, Timer = 1, Level = 2, Assistant = 3 };
constexpr int kHomeAppCount = 4;

// Voice assistant page.
enum class AssistantPhase : uint8_t { Ready, Listening, Thinking, Answer, Error };
enum class AssistantError : uint8_t { None, NotSetUp, NoWifi, Unauthorized, Server, NotHeard, TooShort };
struct AssistantInfo {
  AssistantPhase phase = AssistantPhase::Ready;
  AssistantError error = AssistantError::None;
  int status = 0;          // HTTP status for AssistantError::Server
  int turns = 0;           // questions answered in this conversation
  int recordMs = 0;        // Listening: time so far
  float level = 0;         // Listening: loudness 0..1
  const char *answer = ""; // Answer: UTF-8 text
};

// Spirit level reading (degrees, after calibration).
struct LevelInfo {
  bool sensor = true;   // accelerometer found
  bool edge = false;    // standing on an edge (bar level) instead of lying flat
  bool vertical = false; // edge mode, standing on a side edge (tube runs up/down)
  float x = 0, y = 0;   // flat: tilt left/right and forward/back
  float angle = 0;      // edge: tilt along the tube
  bool calibrated = false;
};

// Kitchen timer.
enum class TimerState : uint8_t { Idle, Running, Paused, Ringing };
struct TimerInfo {
  TimerState state = TimerState::Idle;
  int minutes = 8;      // chosen time (Idle)
  int remainingSec = 0; // Running / Paused
  int totalSec = 0;     // length of the running countdown
};

enum class SetupState : uint8_t { Starting, Ready, Saved };

// Device facts shown in Settings and the setup pages.
struct DeviceInfo {
  char ssid[33] = "";
  int rssi = 0;
  char ip[16] = "";
  int brightnessPercent = 0; // user setting, 5..100
  int volumePercent = 0;     // user setting, 0..100
  bool chimeOn = false;      // hourly chime
  int screenOff = 1;         // index into kScreenOffMinutes
  bool nightDim = true;      // dimmer screen 22:00-07:00
  int savedNetworks = 0;
  // WiFi setup (the board's own access point).
  SetupState setupState = SetupState::Starting;
  char apSsid[33] = "";
  char apPassword[17] = "";
  char savedSsid[33] = "";   // network just saved from the phone
  bool setupOverLan = false; // form served on the home network, not an AP
  int apClients = 0;         // phones joined to the board's own network
  // Saved networks, shown on the WiFi page.
  struct Net {
    char ssid[33];
    bool fromCode; // listed in credentials.h, can't be deleted here
  };
  static constexpr int kMaxNets = 6;
  Net nets[kMaxNets] = {};
  int netCount = 0;
  int pendingDelete = -1; // row waiting for a second tap to delete
};

struct UiState {
  Page page = Page::Home;
  HomeApp app = HomeApp::Weather;
  TimerInfo timer;
  LevelInfo level;
  AssistantInfo assistant;
  int scroll = 0; // Settings list, pixels scrolled down
  ScreenState screen;
  DeviceInfo info;
};

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

inline void drawTitleBar(Canvas &c, const ScreenState &s, const char *title) {
  screen::drawBackChevron(c, 16, 27, kText);
  c.text(kFontTitle, 34, 37, title, kText);
  screen::drawBattery(c, s);
}

// ---------------------------------------------------------------------------
// Home
// ---------------------------------------------------------------------------
constexpr Rect kWeatherTile{24, 196, 152, 168};
constexpr Rect kSettingsTile{192, 196, 152, 168};
constexpr int kTileRadius = 30;
constexpr int kTileLabelBaseline = 398;
// Animated mini weather icon inside the weather tile.
constexpr Rect kTileIconRect{36, 202, 128, 118};
constexpr float kTileIconCx = 100, kTileIconCy = 260, kTileIconSize = 96;

inline void gear(Canvas &c, float cx, float cy, Rgb col, Rgb hole) {
  for (int i = 0; i < 8; i++) {
    const float a = i * 3.14159265f / 4 + 0.39f;
    c.capsule(cx + cosf(a) * 22, cy + sinf(a) * 22, cx + cosf(a) * 31,
              cy + sinf(a) * 31, 6.0f, col);
  }
  c.fillCircle(cx, cy, 25, col);
  c.fillCircle(cx, cy, 10, hole);
}

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

inline void drawWeatherTile(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  // Weather tile, tinted like the weather card.
  const WeatherData &w = s.weather;
  Rgb top, bottom;
  icons::tint(w.valid ? w.code : 3, w.valid ? w.isDay : true, top, bottom);
  const Rect &wt = kWeatherTile;
  c.roundRectGradient(wt.x, wt.y, wt.w, wt.h, kTileRadius, top, bottom);
  char temp[16];
  if (w.valid) {
    screen::formatTemp(temp, sizeof(temp), w.temperature);
  } else {
    snprintf(temp, sizeof(temp), "--");
  }
  c.text(kFontTitle, wt.x + wt.w / 2, wt.y + wt.h - 18, temp, kText, Align::Center);
  c.text(kFontBody, wt.x + wt.w / 2, kTileLabelBaseline, i18n::tr(s.lang, Str::Weather),
         kText, Align::Center);
}

inline const char *appName(Lang L, HomeApp a) {
  switch (a) {
  case HomeApp::Timer: return i18n::tr(L, Str::KitchenTimer);
  case HomeApp::Level: return i18n::tr(L, Str::SpiritLevel);
  case HomeApp::Assistant: return i18n::tr(L, Str::AssistantApp);
  default: return i18n::tr(L, Str::Weather);
  }
}

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

inline void drawHomeStatic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  c.clear(kBackground);
  screen::drawTopBar(c, s);
  screen::drawClock(c, s);

  if (st.app == HomeApp::Timer) drawTimerTile(c, st);
  else if (st.app == HomeApp::Level) drawLevelTile(c, st);
  else if (st.app == HomeApp::Assistant) drawAssistantTile(c, st);
  else drawWeatherTile(c, st);

  // Settings tile.
  const Rect &st2 = kSettingsTile;
  const Rgb sTop = rgb(0x262B34), sBottom = rgb(0x14171C);
  c.roundRectGradient(st2.x, st2.y, st2.w, st2.h, kTileRadius, sTop, sBottom);
  gear(c, st2.x + st2.w / 2, st2.y + st2.h / 2, rgb(0xC9D1DC), mix(sTop, sBottom, 0.5f));
  c.text(kFontBody, st2.x + st2.w / 2, kTileLabelBaseline, i18n::tr(s.lang, Str::Settings),
         kText, Align::Center);
}

inline void drawHomeDynamic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  screen::drawColon(c, s);
  if (st.app == HomeApp::Weather && s.weather.valid) {
    icons::weather(c, kTileIconCx, kTileIconCy, kTileIconSize, s.weather.code,
                   s.weather.isDay, s.moonPhase, s.animTime);
  }
}

// ---------------------------------------------------------------------------
// QR code
// ---------------------------------------------------------------------------
// Draw `text` as a QR code on a white rounded card centered at (cx, cy),
// at most `maxSize` px wide. Returns false if the text didn't fit.
inline bool drawQr(Canvas &c, const char *text, int cx, int cy, int maxSize) {
  static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  static uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_LOW, 1, 10,
                            qrcodegen_Mask_AUTO, true)) {
    return false;
  }
  const int n = qrcodegen_getSize(qr);
  const int quiet = 3;
  const int module = maxSize / (n + quiet * 2);
  const int size = module * (n + quiet * 2);
  const int x0 = cx - size / 2, y0 = cy - size / 2;
  c.roundRect(x0, y0, size, size, module * 2, rgb(0xFFFFFF));
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      if (qrcodegen_getModule(qr, x, y)) {
        c.fillRect(x0 + (x + quiet) * module, y0 + (y + quiet) * module, module, module,
                   rgb(0x000000));
      }
    }
  }
  return true;
}

// Text for a "join this WiFi" QR code (understood by iOS and Android cameras).
inline void wifiQrText(char *out, size_t n, const char *ssid, const char *password) {
  snprintf(out, n, "WIFI:T:WPA;S:%s;P:%s;;", ssid, password);
}

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
constexpr Rgb kDanger = rgb(0xFF6B6B);

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

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
constexpr int kNetRowsTop = 164;
constexpr int kNetRowH = 42;
constexpr int kNetRowGap = 4;
constexpr int kNetRowsMax = 5;
constexpr Rect kAddNetworkButton{44, 394, 280, 44};

inline Rect netRowRect(int i) {
  return Rect{14, kNetRowsTop + i * (kNetRowH + kNetRowGap), 340, kNetRowH};
}
// The "×" delete button at the right end of a row.
inline Rect netDeleteRect(int i) {
  const Rect r = netRowRect(i);
  return Rect{r.x + r.w - 56, r.y, 56, r.h};
}

inline void drawWifiStatic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  const DeviceInfo &d = st.info;
  const Lang L = s.lang;
  c.clear(kBackground);
  drawTitleBar(c, s, i18n::tr(L, Str::Wifi));

  // Status: signal bars, network and address.
  c.roundRect(14, 70, 340, 82, 22, rgb(0x14171D));
  char buf[64];
  if (s.wifiConnected && d.ssid[0]) {
    const int bars = d.rssi > -55 ? 4 : d.rssi > -65 ? 3 : d.rssi > -75 ? 2 : 1;
    for (int i = 0; i < 4; i++) {
      const float h = 8 + i * 6;
      c.roundRect(34 + i * 11, 124 - h, 7, h, 2.5f, i < bars ? kAccent : kFaint);
    }
    snprintf(buf, sizeof(buf), i18n::tr(L, Str::ConnectedTo), d.ssid);
    const bool fits = c.textWidth(kFontBody, buf) <= 250;
    c.text(fits ? kFontBody : kFontSmall, 92, 104, buf, kText);
    snprintf(buf, sizeof(buf), "%d dBm · %s", d.rssi, d.ip);
    c.text(kFontSmall, 92, 130, buf, kMuted);
  } else {
    c.text(kFontBody, W / 2, 118, i18n::tr(L, Str::NotConnected), kText, Align::Center);
  }

  // Saved networks with a delete button (tap twice to delete).
  if (d.netCount == 0) {
    c.text(kFontSmall, W / 2, 196, i18n::tr(L, Str::NoSavedNetworks), kMuted, Align::Center);
  }
  for (int i = 0; i < d.netCount && i < kNetRowsMax; i++) {
    const Rect r = netRowRect(i);
    const bool confirm = d.pendingDelete == i;
    c.roundRect(r.x, r.y, r.w, r.h, 14, confirm ? rgb(0x3A1C1C) : rgb(0x14171D));
    const bool current = s.wifiConnected && strcmp(d.nets[i].ssid, d.ssid) == 0;
    if (current) c.fillCircle(r.x + 18, r.y + r.h / 2, 4, kAccent);
    c.text(kFontBody, r.x + 32, r.y + 28, d.nets[i].ssid, kText);
    if (d.nets[i].fromCode) {
      c.text(kFontSmall, r.x + r.w - 18, r.y + 27, i18n::tr(L, Str::FromCode), kFaint,
             Align::Right);
    } else if (confirm) {
      c.text(kFontSmall, r.x + r.w - 48, r.y + 27, i18n::tr(L, Str::DeleteConfirm),
             screen::kWarn, Align::Right);
      const float cx = r.x + r.w - 26, cy = r.y + r.h / 2.0f;
      c.capsule(cx - 6, cy - 6, cx + 6, cy + 6, 1.6f, screen::kWarn);
      c.capsule(cx - 6, cy + 6, cx + 6, cy - 6, 1.6f, screen::kWarn);
    } else {
      const float cx = r.x + r.w - 26, cy = r.y + r.h / 2.0f;
      c.capsule(cx - 6, cy - 6, cx + 6, cy + 6, 1.6f, kMuted);
      c.capsule(cx - 6, cy + 6, cx + 6, cy - 6, 1.6f, kMuted);
    }
  }

  const Rect &b = kAddNetworkButton;
  c.roundRect(b.x, b.y, b.w, b.h, b.h / 2, rgb(0x1E3A33));
  c.text(kFontBody, b.x + b.w / 2, b.y + 30, i18n::tr(L, Str::AddNetwork), kAccent,
         Align::Center);
}

// Index of the saved network whose "×" was tapped, or -1.
inline int wifiDeleteAt(const UiState &st, int x, int y) {
  if (st.page != Page::Wifi) return -1;
  for (int i = 0; i < st.info.netCount && i < kNetRowsMax; i++) {
    if (!st.info.nets[i].fromCode && inside(netDeleteRect(i), x, y, kNetRowGap / 2)) return i;
  }
  return -1;
}

// QR code for the board's own access point, plus the same details as text.
inline void drawWifiSetupStatic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  const DeviceInfo &d = st.info;
  const Lang L = s.lang;
  c.clear(kBackground);
  drawTitleBar(c, s, i18n::tr(L, Str::Wifi));
  char buf[96];
  if (d.setupState == SetupState::Starting) {
    screen::fitText(c, W / 2, 240, i18n::tr(L, Str::WifiStarting), kMuted);
    return;
  }
  if (d.setupState == SetupState::Saved) {
    snprintf(buf, sizeof(buf), i18n::tr(L, d.setupOverLan ? Str::SavedLan : Str::WifiSaved),
             d.savedSsid);
    screen::fitText(c, W / 2, 240, buf, kText);
    return;
  }
  if (d.setupOverLan) {
    // The board is online: the phone opens its address on the same WiFi.
    snprintf(buf, sizeof(buf), "http://%s/", d.ip);
    drawQr(c, buf, W / 2, 178, 200);
    c.text(kFontSmall, 34, 310, i18n::tr(L, Str::WifiStep1), kText);
    c.text(kFontSmall, 34, 334, i18n::tr(L, Str::SetupLanStep2), kText);
    c.roundRect(14, 352, 340, 82, 20, rgb(0x14171D));
    snprintf(buf, sizeof(buf), i18n::tr(L, Str::ConnectedTo), d.ssid);
    screen::fitText(c, W / 2, 386, buf, kText);
    c.text(kFontSmall, W / 2, 416, i18n::tr(L, Str::SetupLanNote), kMuted, Align::Center);
    return;
  }
  if (d.apClients > 0) {
    // Step 2: the phone is on the board's network; this QR opens the form,
    // for phones that don't pop up the sign-in page by themselves.
    drawQr(c, "http://192.168.4.1/", W / 2, 178, 200);
    c.fillCircle(46, 303, 9, kAccent);
    c.capsule(42, 303, 45, 307, 1.6f, kBackground);
    c.capsule(45, 307, 51, 299, 1.6f, kBackground);
    c.text(kFontBody, 64, 310, i18n::tr(L, Str::PhoneConnected), kAccent);
    c.text(kFontSmall, 34, 340, i18n::tr(L, Str::ScanAgain), kText);
    c.roundRect(14, 360, 340, 74, 20, rgb(0x14171D));
    snprintf(buf, sizeof(buf), i18n::tr(L, Str::OrType), "192.168.4.1");
    c.text(kFontSmall, W / 2, 404, buf, kMuted, Align::Center);
    return;
  }
  wifiQrText(buf, sizeof(buf), d.apSsid, d.apPassword);
  drawQr(c, buf, W / 2, 178, 200);
  c.text(kFontSmall, 34, 310, i18n::tr(L, Str::WifiStep1), kText);
  c.text(kFontSmall, 34, 334, i18n::tr(L, Str::WifiStep2), kText);
  c.roundRect(14, 352, 340, 82, 20, rgb(0x14171D));
  c.text(kFontSmall, 34, 384, i18n::tr(L, Str::NetworkLabel), kMuted);
  c.text(kFontBody, 334, 385, d.apSsid, kText, Align::Right);
  c.text(kFontSmall, 34, 418, i18n::tr(L, Str::PasswordLabel), kMuted);
  c.text(kFontBody, 334, 419, d.apPassword, kText, Align::Right);
}

// ---------------------------------------------------------------------------
// Location
// ---------------------------------------------------------------------------
inline void drawLocationStatic(Canvas &c, const UiState &st) {
  const ScreenState &s = st.screen;
  const DeviceInfo &d = st.info;
  const Lang L = s.lang;
  c.clear(kBackground);
  drawTitleBar(c, s, i18n::tr(L, Str::Location));
  char buf[64];
  if (!s.wifiConnected || !d.ip[0]) {
    screen::fitText(c, W / 2, 240, i18n::tr(L, Str::LocationNeedsWifi), kMuted);
    return;
  }
  snprintf(buf, sizeof(buf), "http://%s/", d.ip);
  drawQr(c, buf, W / 2, 178, 200);
  c.text(kFontSmall, 34, 310, i18n::tr(L, Str::LocationStep1), kText);
  c.text(kFontSmall, 34, 334, i18n::tr(L, Str::LocationStep2), kText);
  c.roundRect(14, 352, 340, 82, 20, rgb(0x14171D));
  snprintf(buf, sizeof(buf), i18n::tr(L, Str::Current), s.city);
  screen::fitText(c, W / 2, 386, buf, kText);
  c.text(kFontSmall, W / 2, 416, i18n::tr(L, Str::LocationSameWifi), kMuted, Align::Center);
}

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

inline void pill(Canvas &c, const Rect &r, Rgb bg) {
  c.roundRect(r.x, r.y, r.w, r.h, r.h / 2.0f, bg);
}

inline void pillLabel(Canvas &c, const Rect &r, const Font &f, const char *text, Rgb col,
                      int dy = 7) {
  c.text(f, r.x + r.w / 2, r.y + r.h / 2 + dy, text, col, Align::Center);
}

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
    // Glass highlight along the tube.
    if (l.vertical) {
      c.capsule(t.x + 13, t.y + 44, t.x + 13, t.y + 110, 2, rgb(0xFFFFFF), 0.07f);
    } else {
      c.capsule(t.x + 44, t.y + 13, t.x + 110, t.y + 13, 2, rgb(0xFFFFFF), 0.07f);
    }
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
