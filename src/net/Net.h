#pragma once

// Networking: WiFi, weather, time zone and the phone-facing web pages.
//
// Everything runs on one background task so the UI never waits for the
// network. The UI reads snapshots through the functions below.

#include "ui/Screen.h"
#include "ui/Ui.h"
#include <stddef.h>

enum class WifiCode : uint8_t {
  Connecting,    // starting up
  ConnectingTo,  // trying `ssid`
  CantFind,      // `ssid` not in range (or 5 GHz only)
  WrongPassword, // `ssid` rejected the password
  Unavailable,   // `ssid` failed for another reason
  NoNetworks,    // nothing saved yet: set up WiFi in Settings
  Connected,
};

struct WifiState {
  WifiCode code = WifiCode::Connecting;
  char ssid[33] = "";
};

struct Place {
  char name[48] = "";
  float lat = 0;
  float lon = 0;
};

namespace net {

// Load saved networks, place and time zone; start the network task.
void begin();

// Weather.
void requestRefresh();
bool refreshing();
void weather(WeatherData &out, bool &lastFailed, unsigned long &lastGoodMs);

// Status for the screen.
WifiState wifiState();
Place place();
int savedNetworkCount();

// WiFi setup through the board's own access point (while the setup page is
// open on the device).
void setSetupWanted(bool on);
bool setupActive();
SetupState setupState();
void setupCredentials(char *ssid, size_t ssidLen, char *password, size_t passLen,
                      char *savedSsid, size_t savedLen);

// True when setup runs over the home network (QR = the board's address)
// instead of the board's own access point.
bool setupOverLan();
// Phones connected to the board's own network during setup.
int setupClients();

// Saved networks: from the phone (can be forgotten) and from credentials.h.
struct NetEntry {
  char ssid[33];
  bool fromCode;
};
int networks(NetEntry *out, int max);
void forget(const char *ssid);

// Location web page on the home network (while the Location page is open).
void setWebWanted(bool on);

// Screen off: keep WiFi off except for the periodic refresh (every
// WEATHER_REFRESH_MS). Switching it off again reconnects right away.
void setLowPower(bool on);
// True while the radio is off for low power or reconnecting after it; the
// UI treats that as connected so no WiFi warning flashes up.
bool radioResting();
// True while low power has the radio switched off and no refresh is due, so
// the chip may light-sleep.
bool radioAsleep();

// Language of the phone pages.
void setPortalCroatian(bool hr);

// New POSIX time zone for the clock, if it changed since the last call.
bool takeTimezone(char *out, size_t n);

} // namespace net
