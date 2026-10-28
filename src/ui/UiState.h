#pragma once

// Everything the UI shows, as one snapshot that main.cpp builds every frame:
// the page, the home-screen app and the state of each app.

#include "ui/Screen.h"

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
