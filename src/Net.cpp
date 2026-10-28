#include "Net.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_random.h>
#include <esp_sntp.h>
#include <utility>
#include <time.h>

#include "Config.h"
#include "Portal.h"

// credentials.h is optional: networks can also be added from a phone.
#if __has_include("credentials.h")
#include "credentials.h"
#define HAVE_CREDENTIALS_H 1
#endif

namespace net {
namespace {

constexpr int kMaxSaved = 5;

struct Network {
  char ssid[33];
  char pass[65];
};

// --- State shared with the UI task (guarded by `lock`) ----------------------
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
WeatherData gWeather;
unsigned long gLastGoodMs = 0;
bool gLastFailed = false;
WifiState gWifi;
Place gPlace;
int gSavedCount = 0;
SetupState gSetupState = SetupState::Starting;
char gApSsid[33] = "";
char gApPass[17] = "";
char gSavedSsid[33] = "";
char gTz[40] = "";
bool gTzChanged = false;

volatile bool gRefreshRequested = true;
volatile bool gRefreshing = false;
volatile bool gSetupWanted = false;
bool gSetupLan = false;            // setup runs over the home network
int gApClients = 0;                // phones connected to the board's own network
char gSavedNames[kMaxSaved][33] = {};
volatile bool gForgetRequested = false;
char gForgetSsid[33] = "";
volatile bool gWebWanted = false;
volatile bool gPortalHr = false;
volatile int gDisconnectReason = 0;
// Screen off: WiFi only for the periodic refresh, radio off in between.
volatile bool gLowPower = false;
// The radio is off on purpose (low power), or reconnecting after it was:
// the UI keeps showing "connected" instead of a WiFi warning.
volatile bool gRadioResting = false;

// --- Owned by the network task ---------------------------------------------
Preferences prefs;
Network saved[kMaxSaved];
int savedCount = 0;
bool tzFromIp = false;

WebServer server(80);
DNSServer dns;
// Lan: the board stays on its WiFi and serves the form at its own address.
// Ap: the board isn't connected anywhere, so it opens its own network.
enum class SetupMode { None, Lan, Ap };
SetupMode setupMode = SetupMode::None;
bool serverRunning = false;
volatile bool radioOff = false; // switched off by low power
unsigned long setupStartedMs = 0;
String setupOptions; // nearby network names (as tappable chips)
bool pendingSave = false;
unsigned long pendingSaveMs = 0;
Network pendingNetwork;
const char *preferredSsid = nullptr; // try this one first after setup

unsigned long nextConnectMs = 0;
unsigned long nextFetchMs = 0;
unsigned long nextTzMs = 0;

template <typename F> void locked(F f) {
  portENTER_CRITICAL(&lock);
  f();
  portEXIT_CRITICAL(&lock);
}

void setWifi(WifiCode code, const char *ssid) {
  locked([&] {
    gWifi.code = code;
    snprintf(gWifi.ssid, sizeof(gWifi.ssid), "%s", ssid ? ssid : "");
  });
}

// --- Settings ----------------------------------------------------------------
void loadSettings() {
  prefs.begin("net", false);
  savedCount = min<int>(prefs.getUChar("count", 0), kMaxSaved);
  for (int i = 0; i < savedCount; i++) {
    char key[8];
    snprintf(key, sizeof(key), "s%d", i);
    prefs.getString(key, saved[i].ssid, sizeof(saved[i].ssid));
    snprintf(key, sizeof(key), "p%d", i);
    prefs.getString(key, saved[i].pass, sizeof(saved[i].pass));
  }
  Place p;
  if (prefs.isKey("pname")) {
    prefs.getString("pname", p.name, sizeof(p.name));
    p.lat = prefs.getFloat("plat", 0);
    p.lon = prefs.getFloat("plon", 0);
  } else {
    snprintf(p.name, sizeof(p.name), "%s", DEFAULT_PLACE_NAME);
    p.lat = DEFAULT_PLACE_LAT;
    p.lon = DEFAULT_PLACE_LON;
  }
  char tz[40];
  if (prefs.isKey("tz")) {
    prefs.getString("tz", tz, sizeof(tz));
  } else {
    snprintf(tz, sizeof(tz), "%s", DEFAULT_TZ);
  }
  locked([&] {
    gPlace = p;
    gSavedCount = savedCount;
    for (int i = 0; i < savedCount; i++) {
      snprintf(gSavedNames[i], sizeof(gSavedNames[i]), "%s", saved[i].ssid);
    }
    snprintf(gTz, sizeof(gTz), "%s", tz);
    gTzChanged = true;
  });
}

void persistNetworks() {
  prefs.putUChar("count", savedCount);
  for (int i = 0; i < savedCount; i++) {
    char key[8];
    snprintf(key, sizeof(key), "s%d", i);
    prefs.putString(key, saved[i].ssid);
    snprintf(key, sizeof(key), "p%d", i);
    prefs.putString(key, saved[i].pass);
  }
  locked([&] {
    gSavedCount = savedCount;
    for (int i = 0; i < kMaxSaved; i++) {
      snprintf(gSavedNames[i], sizeof(gSavedNames[i]), "%s", i < savedCount ? saved[i].ssid : "");
    }
  });
}

// Newest network goes first; an existing entry is updated and moved up.
void rememberNetwork(const Network &n) {
  int at = -1;
  for (int i = 0; i < savedCount; i++) {
    if (strcmp(saved[i].ssid, n.ssid) == 0) at = i;
  }
  if (at < 0) at = savedCount < kMaxSaved ? savedCount++ : kMaxSaved - 1;
  for (int i = at; i > 0; i--) saved[i] = saved[i - 1];
  saved[0] = n;
  persistNetworks();
  Serial.printf("[Net] saved network \"%s\" (%d stored)\n", n.ssid, savedCount);
}

// Networks listed in credentials.h are copied into the saved list once, so
// they can be managed (and deleted for good) on the device like any other.
// The names already imported are remembered, so a deleted one stays deleted.
void importFromHeader() {
#ifdef HAVE_CREDENTIALS_H
  String imported = prefs.getString("imported", "\n");
  bool changed = false;
  for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
    if (!WIFI_NETWORKS[i].ssid[0]) continue; // empty example line
    const String key = String(WIFI_NETWORKS[i].ssid) + "\n";
    if (imported.indexOf("\n" + key) >= 0) continue;
    bool exists = false;
    for (int j = 0; j < savedCount; j++) exists |= strcmp(saved[j].ssid, WIFI_NETWORKS[i].ssid) == 0;
    if (!exists && savedCount < kMaxSaved) {
      Network &n = saved[savedCount++];
      snprintf(n.ssid, sizeof(n.ssid), "%s", WIFI_NETWORKS[i].ssid);
      snprintf(n.pass, sizeof(n.pass), "%s", WIFI_NETWORKS[i].password);
      Serial.printf("[Net] imported \"%s\" from credentials.h\n", n.ssid);
    }
    imported += key;
    changed = true;
  }
  if (changed) {
    prefs.putString("imported", imported);
    persistNetworks();
  }
#endif
}

void forgetNetwork(const char *ssid) {
  int at = -1;
  for (int i = 0; i < savedCount; i++) {
    if (strcmp(saved[i].ssid, ssid) == 0) at = i;
  }
  if (at < 0) return;
  for (int i = at; i < savedCount - 1; i++) saved[i] = saved[i + 1];
  savedCount--;
  persistNetworks();
  Serial.printf("[Net] forgot network \"%s\" (%d stored)\n", ssid, savedCount);
  // If that's the network in use, drop it so the board moves to another one.
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) {
    WiFi.disconnect();
    nextConnectMs = 0;
  }
}

// POSIX TZ for a fixed UTC offset, e.g. 7200 -> "<+0200>-2". Refreshed
// regularly, so summer/winter time changes are picked up automatically.
void posixFromOffset(int seconds, char *out, size_t n) {
  const int abs = seconds < 0 ? -seconds : seconds;
  const int h = abs / 3600, m = (abs % 3600) / 60;
  const char name = seconds >= 0 ? '+' : '-';
  const char posix = seconds >= 0 ? '-' : '+'; // POSIX counts west as positive
  if (m) snprintf(out, n, "<%c%02d%02d>%c%d:%02d", name, h, m, posix, h, m);
  else snprintf(out, n, "<%c%02d%02d>%c%d", name, h, m, posix, h);
}

// Full POSIX rules (with the exact summer/winter switch) for common zones.
// Other zones fall back to the current fixed offset, refreshed hourly.
const char *posixForZone(const char *iana) {
  struct Zone {
    const char *rule;
    const char *names;
  };
  static const Zone kZones[] = {
      {"CET-1CEST,M3.5.0,M10.5.0/3",
       "Zagreb Belgrade Sarajevo Ljubljana Podgorica Skopje Vienna Berlin Rome Paris "
       "Madrid Amsterdam Brussels Budapest Prague Warsaw Bratislava Stockholm Oslo "
       "Copenhagen Zurich Luxembourg Monaco Malta Andorra San_Marino Vatican Tirane "
       "Gibraltar Busingen Vaduz"},
      {"EET-2EEST,M3.5.0/3,M10.5.0/4",
       "Athens Helsinki Kyiv Kiev Riga Tallinn Vilnius Sofia Bucharest Chisinau Nicosia "
       "Mariehamn Uzhgorod Zaporozhye"},
      {"GMT0BST,M3.5.0/1,M10.5.0", "London Jersey Guernsey Isle_of_Man"},
      {"IST-1GMT0,M10.5.0,M3.5.0/1", "Dublin"},
      {"WET0WEST,M3.5.0/1,M10.5.0", "Lisbon"},
  };
  static const struct {
    const char *iana, *rule;
  } kAmerica[] = {
      {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
      {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
      {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
      {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
      {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
      {"America/Vancouver", "PST8PDT,M3.2.0,M11.1.0"},
  };
  if (strncmp(iana, "Europe/", 7) == 0) {
    const char *city = iana + 7;
    const size_t len = strlen(city);
    for (const Zone &z : kZones) {
      const char *p = z.names;
      while ((p = strstr(p, city)) != nullptr) {
        const bool startOk = p == z.names || p[-1] == ' ';
        const bool endOk = p[len] == ' ' || p[len] == 0;
        if (startOk && endOk) return z.rule;
        p += len;
      }
    }
  }
  for (const auto &a : kAmerica) {
    if (strcmp(iana, a.iana) == 0) return a.rule;
  }
  return nullptr;
}

void applyTimezone(const char *tz, bool persist) {
  bool changed = false;
  locked([&] {
    if (strcmp(gTz, tz) != 0) {
      snprintf(gTz, sizeof(gTz), "%s", tz);
      gTzChanged = true;
      changed = true;
    }
  });
  if (changed) {
    Serial.printf("[Net] time zone %s\n", tz);
    if (persist) prefs.putString("tz", tz);
  }
}

// The phone's offset is only a fallback for when the IP lookup hasn't worked.
void phoneTimezoneFallback(const String &offset) {
  if (tzFromIp || offset.isEmpty()) return;
  const int sec = offset.toInt();
  if (sec < -14 * 3600 || sec > 14 * 3600) return;
  char tz[40];
  posixFromOffset(sec, tz, sizeof(tz));
  applyTimezone(tz, true);
}

// --- WiFi ----------------------------------------------------------------------
int candidates(const Network **out, int max) {
  int n = 0;
  auto add = [&](const Network *net) {
    for (int i = 0; i < n; i++) {
      if (strcmp(out[i]->ssid, net->ssid) == 0) return;
    }
    if (n < max) out[n++] = net;
  };
  if (preferredSsid) {
    for (int i = 0; i < savedCount; i++) {
      if (strcmp(saved[i].ssid, preferredSsid) == 0) add(&saved[i]);
    }
  }
  for (int i = 0; i < savedCount; i++) add(&saved[i]);
  return n;
}

bool isAuthFailure(int reason) {
  return reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
         reason == WIFI_REASON_HANDSHAKE_TIMEOUT || reason == WIFI_REASON_AUTH_EXPIRE;
}

bool connectWiFi() {
  const Network *list[kMaxSaved + 8];
  const int n = candidates(list, kMaxSaved + 8);
  if (n == 0) {
    setWifi(WifiCode::NoNetworks, "");
    return false;
  }
  WiFi.mode(WIFI_STA);
  int lastReason = 0;
  const char *lastSsid = "";
  for (int i = 0; i < n; i++) {
    const char *ssid = list[i]->ssid;
    Serial.printf("[WiFi] connecting to \"%s\"\n", ssid);
    setWifi(WifiCode::ConnectingTo, ssid);
    gDisconnectReason = 0;
    WiFi.begin(ssid, list[i]->pass);
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS &&
           !gSetupWanted) {
      vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] connected to \"%s\", IP %s, %d dBm\n", ssid,
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      setWifi(WifiCode::Connected, ssid);
      char tz[40];
      locked([&] { snprintf(tz, sizeof(tz), "%s", gTz); });
      configTzTime(tz, NTP_SERVER_1, NTP_SERVER_2);
      preferredSsid = nullptr;
      return true;
    }
    lastReason = gDisconnectReason;
    lastSsid = ssid;
    Serial.printf("[WiFi] \"%s\" failed: reason %d\n", ssid, lastReason);
    WiFi.disconnect();
    if (gSetupWanted) return false;
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  if (lastReason == WIFI_REASON_NO_AP_FOUND) setWifi(WifiCode::CantFind, lastSsid);
  else if (isAuthFailure(lastReason)) setWifi(WifiCode::WrongPassword, lastSsid);
  else setWifi(WifiCode::Unavailable, lastSsid);
  return false;
}

// --- Time zone by IP -------------------------------------------------------------
bool lookupTimezone() {
  HTTPClient http;
  // ip-api.com: free for non-commercial use, HTTP only, no key.
  if (!http.begin("http://ip-api.com/json/?fields=status,offset,timezone")) return false;
  http.setTimeout(6000);
  const int status = http.GET();
  if (status != 200) {
    http.end();
    return false;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err || strcmp(doc["status"] | "", "success") != 0 || doc["offset"].isNull()) return false;
  char tz[40];
  const char *rule = posixForZone(doc["timezone"] | "");
  if (rule) snprintf(tz, sizeof(tz), "%s", rule);
  else posixFromOffset(doc["offset"].as<int>(), tz, sizeof(tz));
  Serial.printf("[Net] IP time zone %s (%s)\n", doc["timezone"] | "?", tz);
  tzFromIp = true;
  applyTimezone(tz, true);
  return true;
}

// --- Weather ------------------------------------------------------------------
int weekdayOf(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[(m - 1) % 12] + d) % 7;
}

// "2026-10-22T07:12" -> "07:12"
void copyClock(char *dst, size_t n, const char *iso) {
  if (!iso || strlen(iso) < 16) {
    dst[0] = 0;
    return;
  }
  snprintf(dst, n, "%.5s", iso + 11);
}

bool fetchWeather(const Place &p, WeatherData &out) {
  WiFiClientSecure client;
  client.setInsecure(); // public weather data, no secrets involved
  HTTPClient http;
  char url[400];
  // timezone=auto: sunrise/sunset in the place's own local time.
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
           "weather_code,is_day,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
           "precipitation_probability_max,sunrise,sunset"
           "&timezone=auto&forecast_days=4",
           p.lat, p.lon);
  if (!http.begin(client, url)) return false;
  http.setTimeout(10000);
  const int status = http.GET();
  if (status != 200) {
    Serial.printf("[Weather] HTTP %d\n", status);
    http.end();
    return false;
  }
  const String body = http.getString(); // also handles chunked responses
  http.end();
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[Weather] JSON error: %s\n", err.c_str());
    return false;
  }
  JsonObject cur = doc["current"];
  JsonObject daily = doc["daily"];
  if (cur.isNull() || cur["weather_code"].isNull()) return false;

  out.valid = true;
  out.temperature = cur["temperature_2m"] | 0.0f;
  out.feelsLike = cur["apparent_temperature"] | out.temperature;
  out.humidity = cur["relative_humidity_2m"] | 0;
  out.windKmh = cur["wind_speed_10m"] | 0.0f;
  out.code = cur["weather_code"] | -1;
  out.isDay = (cur["is_day"] | 1) == 1;
  out.todayMin = daily["temperature_2m_min"][0] | out.temperature;
  out.todayMax = daily["temperature_2m_max"][0] | out.temperature;
  copyClock(out.sunrise, sizeof(out.sunrise), daily["sunrise"][0].as<const char *>());
  copyClock(out.sunset, sizeof(out.sunset), daily["sunset"][0].as<const char *>());
  out.dayCount = 0;
  for (int i = 1; i <= 3; i++) {
    const char *date = daily["time"][i] | "";
    if (strlen(date) < 10 || daily["weather_code"][i].isNull()) break;
    DayForecast &d = out.days[out.dayCount++];
    d.code = daily["weather_code"][i] | -1;
    d.tempMax = daily["temperature_2m_max"][i] | 0.0f;
    d.tempMin = daily["temperature_2m_min"][i] | 0.0f;
    d.precipChance = daily["precipitation_probability_max"][i].isNull()
                         ? -1
                         : daily["precipitation_probability_max"][i].as<int>();
    const int year = atoi(date), month = atoi(date + 5), day = atoi(date + 8);
    d.day = day;
    d.month = month;
    d.weekday = weekdayOf(year, month, day);
  }
  Serial.printf("[Weather] %s: %.1f C code=%d day=%d\n", p.name, out.temperature, out.code,
                out.isDay ? 1 : 0);
  return true;
}

// --- Web pages -------------------------------------------------------------------
String fill(const char *tpl) {
  String s(tpl);
  s.replace("%STYLE%", portal::kStyle);
  return s;
}

void sendWifiPage() {
  const portal::Texts &t = portal::texts(gPortalHr);
  String s = fill(portal::kWifiPage);
  s.replace("%TITLE%", t.wifiTitle);
  s.replace("%SUB%", t.wifiSub);
  s.replace("%NETWORK%", t.network);
  s.replace("%HINT%", t.hint);
  s.replace("%NEARBY%", t.nearby);
  s.replace("%PASSWORD%", t.password);
  s.replace("%SAVE%", t.save);
  s.replace("%CHIPS%", setupOptions);
  server.send(200, "text/html; charset=utf-8", s);
}

void sendLocationPage() {
  const portal::Texts &t = portal::texts(gPortalHr);
  Place p;
  locked([&] { p = gPlace; });
  String s = fill(portal::kLocationPage);
  s.replace("%TITLE%", t.locTitle);
  s.replace("%NOW%", t.now);
  s.replace("%CITY%", portal::escape(p.name));
  s.replace("%PLACEHOLDER%", t.placeholder);
  s.replace("%SEARCH%", t.search);
  s.replace("%NONE%", t.none);
  s.replace("%OFFLINE%", t.offline);
  s.replace("%SAVED%", t.saved);
  s.replace("%FAILED%", t.failed);
  s.replace("%LANG%", t.lang);
  server.send(200, "text/html; charset=utf-8", s);
}

void redirectToPortal() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

void logRequest() {
  Serial.printf("[Web] %s %s\n", server.method() == HTTP_POST ? "POST" : "GET",
                server.uri().c_str());
}

void handleRoot() {
  logRequest();
  if (setupMode != SetupMode::None) sendWifiPage();
  else sendLocationPage();
}

void handleSave() {
  logRequest();
  if (setupMode == SetupMode::None) {
    server.send(404, "text/plain", "");
    return;
  }
  String ssid = server.arg("ssid");
  ssid.trim();
  const String pass = server.arg("pass");
  if (ssid.isEmpty() || ssid.length() > 32 || pass.length() > 63 ||
      (pass.length() > 0 && pass.length() < 8)) {
    sendWifiPage();
    return;
  }
  phoneTimezoneFallback(server.arg("tz"));
  snprintf(pendingNetwork.ssid, sizeof(pendingNetwork.ssid), "%s", ssid.c_str());
  snprintf(pendingNetwork.pass, sizeof(pendingNetwork.pass), "%s", pass.c_str());
  pendingSave = true;
  pendingSaveMs = millis();
  const portal::Texts &t = portal::texts(gPortalHr);
  String s = fill(portal::kWifiSaved);
  s.replace("%DONE%", t.done);
  s.replace("%NEXT%", setupMode == SetupMode::Lan ? t.nextLan : t.next);
  server.send(200, "text/html; charset=utf-8", s);
}

void handleLocation() {
  if (setupMode == SetupMode::Ap) {
    server.send(404, "text/plain", "");
    return;
  }
  String name = server.arg("name");
  name.trim();
  const float lat = server.arg("lat").toFloat();
  const float lon = server.arg("lon").toFloat();
  if (name.isEmpty() || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
    server.send(400, "text/plain", "bad place");
    return;
  }
  Place p;
  snprintf(p.name, sizeof(p.name), "%s", name.c_str());
  p.lat = lat;
  p.lon = lon;
  prefs.putString("pname", p.name);
  prefs.putFloat("plat", lat);
  prefs.putFloat("plon", lon);
  locked([&] {
    gPlace = p;
    gWeather = WeatherData(); // old place's weather no longer applies
  });
  phoneTimezoneFallback(server.arg("tz"));
  gRefreshRequested = true;
  Serial.printf("[Net] place %s (%.4f, %.4f)\n", p.name, lat, lon);
  server.send(200, "text/plain", "ok");
}

void handleNotFound() {
  logRequest();
  if (setupMode == SetupMode::Ap) redirectToPortal();
  else server.send(404, "text/plain", "");
}

void setupRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/api/location", HTTP_POST, handleLocation);
  server.on("/favicon.ico", HTTP_GET, [] { server.send(204, "text/plain", ""); });
  // Phones probe these to detect a captive portal; answer with the setup page.
  for (const char *probe : {"/generate_204", "/gen_204", "/hotspot-detect.html",
                            "/connecttest.txt", "/ncsi.txt", "/fwlink", "/redirect"}) {
    server.on(probe, HTTP_GET, [] {
      logRequest();
      if (setupMode == SetupMode::Ap) redirectToPortal();
      else server.send(404, "text/plain", "");
    });
  }
  server.onNotFound(handleNotFound);
}

// --- Setup mode (the board's own access point) -------------------------------------
void buildOptions() {
  const int n = WiFi.scanNetworks();
  setupOptions = "";
  // Strongest first, each name once.
  int order[30];
  int count = 0;
  for (int i = 0; i < n && count < 30; i++) {
    if (WiFi.SSID(i).isEmpty()) continue;
    bool dup = false;
    for (int j = 0; j < count; j++) dup |= WiFi.SSID(order[j]) == WiFi.SSID(i);
    if (!dup) order[count++] = i;
  }
  for (int a = 0; a < count; a++) {
    for (int b = a + 1; b < count; b++) {
      if (WiFi.RSSI(order[b]) > WiFi.RSSI(order[a])) std::swap(order[a], order[b]);
    }
  }
  for (int k = 0; k < count && k < 12; k++) {
    setupOptions += "<span class=\"chip\">" + portal::escape(WiFi.SSID(order[k])) + "</span>";
  }
  WiFi.scanDelete();
  Serial.printf("[Setup] %d networks in the list\n", count);
}

void startSetup() {
  locked([&] { gSetupState = SetupState::Starting; gSavedSsid[0] = 0; });
  pendingSave = false;
  setupStartedMs = millis();

  if (WiFi.status() == WL_CONNECTED) {
    // Already online: keep the phone on the same WiFi and serve the form
    // at the board's address. Needed to add e.g. the phone's own hotspot.
    Serial.println("[Setup] over the home network");
    buildOptions();
    if (!serverRunning) {
      server.begin();
      serverRunning = true;
    }
    setupMode = SetupMode::Lan;
    locked([&] {
      gSetupLan = true;
      gApSsid[0] = gApPass[0] = 0;
      gSetupState = SetupState::Ready;
    });
    Serial.printf("[Setup] form at http://%s/\n", WiFi.localIP().toString().c_str());
    return;
  }

  Serial.println("[Setup] starting access point");
  if (serverRunning) server.stop();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  buildOptions();

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ssid[33], pass[17];
  snprintf(ssid, sizeof(ssid), "%s-%02X%02X", SETUP_AP_PREFIX, mac[4], mac[5]);
  // Random password without look-alike characters (no 0/o, 1/l/i).
  static const char kChars[] = "abcdefghjkmnpqrstuvwxyz23456789";
  for (int i = 0; i < 8; i++) pass[i] = kChars[esp_random() % (sizeof(kChars) - 1)];
  pass[8] = 0;
  WiFi.softAP(ssid, pass);
  delay(100);
  dns.start(53, "*", WiFi.softAPIP());
  server.begin();
  serverRunning = true;
  setupMode = SetupMode::Ap;
  setupStartedMs = millis();
  locked([&] {
    gSetupLan = false;
    gApClients = 0;
    snprintf(gApSsid, sizeof(gApSsid), "%s", ssid);
    snprintf(gApPass, sizeof(gApPass), "%s", pass);
    gSetupState = SetupState::Ready;
  });
  setWifi(WifiCode::Connecting, "");
  Serial.printf("[Setup] AP \"%s\" at %s\n", ssid, WiFi.softAPIP().toString().c_str());
}

void stopSetup() {
  if (setupMode == SetupMode::Ap) {
    Serial.println("[Setup] stopping access point");
    dns.stop();
    server.stop();
    serverRunning = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    nextConnectMs = 0;
  } else {
    Serial.println("[Setup] done");
    // The web server stays up only if the Location page still wants it.
  }
  setupMode = SetupMode::None;
}

void networkTask(void *) {
  Serial.println("[Net] task started");
  WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
    gDisconnectReason = info.wifi_sta_disconnected.reason;
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  setupRoutes();

  for (;;) {
    const unsigned long now = millis();

    // --- Requests from the UI --------------------------------------------------
    if (gForgetRequested) {
      char ssid[33];
      locked([&] {
        snprintf(ssid, sizeof(ssid), "%s", gForgetSsid);
        gForgetRequested = false;
      });
      forgetNetwork(ssid);
    }

    // --- Setup mode ---------------------------------------------------------
    if (gSetupWanted && setupMode == SetupMode::None) startSetup();
    if (!gSetupWanted && setupMode != SetupMode::None) stopSetup();
    if (setupMode == SetupMode::Ap) {
      dns.processNextRequest();
      server.handleClient();
      // Notice when a phone joins, so the screen can switch to the second QR
      // code (the page address) in case the phone doesn't open it by itself.
      static unsigned long lastClientCheck = 0;
      if (millis() - lastClientCheck > 500) {
        lastClientCheck = millis();
        const int clients = WiFi.softAPgetStationNum();
        int before;
        locked([&] {
          before = gApClients;
          gApClients = clients;
        });
        if (clients != before) Serial.printf("[Setup] %d phone(s) connected\n", clients);
      }
    }
    if (setupMode != SetupMode::None) {
      // Fresh time: `now` was taken before startSetup()/handleClient(), which
      // can take seconds, and these timestamps may be newer than it.
      const unsigned long t = millis();
      if (pendingSave && t - pendingSaveMs > 1500) {
        // Let the phone receive the confirmation before anything changes.
        pendingSave = false;
        rememberNetwork(pendingNetwork);
        // Over the access point there's no connection yet: try the new one
        // first. Over the home network, stay on the current connection.
        if (setupMode == SetupMode::Ap) preferredSsid = saved[0].ssid;
        locked([&] {
          gSetupState = SetupState::Saved;
          snprintf(gSavedSsid, sizeof(gSavedSsid), "%s", pendingNetwork.ssid);
        });
        gSetupWanted = false;
        stopSetup();
      } else if (t - setupStartedMs > SETUP_TIMEOUT_MS) {
        Serial.println("[Setup] timed out");
        gSetupWanted = false;
        stopSetup();
      }
    }
    if (setupMode == SetupMode::Ap) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    // --- Normal mode --------------------------------------------------------
    // Low power with the radio off: wait until the next refresh is due.
    if (radioOff) {
      const bool due = !gLowPower || gRefreshRequested || (long)(now - nextFetchMs) >= 0;
      if (!due) {
        // Short waits: the chip light-sleeps in between and only runs this
        // task for a moment after each wake-up.
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }
      radioOff = false;
      nextConnectMs = 0;
      Serial.println(gLowPower ? "[Power] WiFi on for the refresh" : "[Power] WiFi on");
    }
    if (WiFi.status() != WL_CONNECTED) {
      if (serverRunning) {
        server.stop();
        serverRunning = false;
      }
      if (now >= nextConnectMs) {
        if (connectWiFi()) {
          // A reconnect after a low-power pause: the forecast is only stale
          // if its refresh is due; the time zone keeps its hourly schedule.
          if (!gRadioResting) {
            nextTzMs = 0;
            gRefreshRequested = true;
          }
          gRadioResting = false;
        } else {
          gRadioResting = false;
          // On battery with the screen off, don't keep the radio busy.
          nextConnectMs = millis() + (gLowPower ? 5UL * 60UL * 1000UL : WIFI_RETRY_MS);
          if (gLowPower) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            radioOff = true;
            nextFetchMs = nextConnectMs;
          }
        }
      }
    } else {
      if (now >= nextTzMs) {
        nextTzMs = now + (lookupTimezone() ? TZ_LOOKUP_MS : 60000UL);
      }
      if (gRefreshRequested || now >= nextFetchMs) {
        gRefreshRequested = false;
        gRefreshing = true;
        Place p;
        locked([&] { p = gPlace; });
        WeatherData fresh;
        const bool ok = fetchWeather(p, fresh);
        locked([&] {
          if (ok) {
            gWeather = fresh;
            gLastGoodMs = millis();
          }
          gLastFailed = !ok;
        });
        nextFetchMs = millis() + (ok ? WEATHER_REFRESH_MS : WEATHER_RETRY_MS);
        gRefreshing = false;
      }
      const bool webWanted = gWebWanted || setupMode == SetupMode::Lan;
      if (webWanted && !serverRunning) {
        server.begin();
        serverRunning = true;
        Serial.printf("[Net] web page at http://%s/\n", WiFi.localIP().toString().c_str());
      } else if (!webWanted && serverRunning) {
        server.stop();
        serverRunning = false;
      }
      if (serverRunning) server.handleClient();

      // Low power: once the refresh is done (and the clock had a moment to
      // sync), switch the radio off until the next one.
      if (gLowPower && !gRefreshRequested && !gRefreshing && !serverRunning &&
          (long)(millis() - nextFetchMs) < 0) {
        const unsigned long waitStart = millis();
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS &&
               millis() - waitStart < 3000) {
          vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (gLowPower) {
          Serial.printf("[Power] WiFi off, next refresh in %lu s\n",
                        (nextFetchMs - millis()) / 1000);
          gRadioResting = true;
          WiFi.disconnect(true);
          WiFi.mode(WIFI_OFF);
          radioOff = true;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(gLowPower ? 50 : 10));
  }
}

} // namespace

void begin() {
  loadSettings();
  importFromHeader();
  xTaskCreatePinnedToCore(networkTask, "net", 16384, nullptr, 1, nullptr, 0);
}

void requestRefresh() { gRefreshRequested = true; }
bool refreshing() { return gRefreshing || gRefreshRequested; }

void weather(WeatherData &out, bool &lastFailed, unsigned long &lastGoodMs) {
  locked([&] {
    out = gWeather;
    lastFailed = gLastFailed;
    lastGoodMs = gLastGoodMs;
  });
}

WifiState wifiState() {
  WifiState s;
  locked([&] { s = gWifi; });
  return s;
}

Place place() {
  Place p;
  locked([&] { p = gPlace; });
  return p;
}

int savedNetworkCount() {
  int n;
  locked([&] { n = gSavedCount; });
  return n;
}

void setSetupWanted(bool on) {
  if (on) locked([&] { gSetupState = SetupState::Starting; });
  gSetupWanted = on;
}
bool setupActive() { return gSetupWanted; }

SetupState setupState() {
  SetupState s;
  locked([&] { s = gSetupState; });
  return s;
}

void setupCredentials(char *ssid, size_t ssidLen, char *password, size_t passLen,
                      char *savedSsid, size_t savedLen) {
  locked([&] {
    snprintf(ssid, ssidLen, "%s", gApSsid);
    snprintf(password, passLen, "%s", gApPass);
    snprintf(savedSsid, savedLen, "%s", gSavedSsid);
  });
}

int setupClients() {
  int n;
  locked([&] { n = gApClients; });
  return n;
}

bool setupOverLan() {
  bool lan;
  locked([&] { lan = gSetupLan; });
  return lan;
}

int networks(NetEntry *out, int max) {
  int n = 0;
  locked([&] {
    for (int i = 0; i < gSavedCount && n < max; i++) {
      snprintf(out[n].ssid, sizeof(out[n].ssid), "%s", gSavedNames[i]);
      out[n++].fromCode = false;
    }
  });
  return n;
}

void forget(const char *ssid) {
  locked([&] {
    snprintf(gForgetSsid, sizeof(gForgetSsid), "%s", ssid);
    gForgetRequested = true;
  });
}

void setWebWanted(bool on) { gWebWanted = on; }

void setLowPower(bool on) { gLowPower = on; }
bool radioResting() { return gRadioResting; }
bool radioAsleep() {
  return radioOff && gLowPower && !gRefreshRequested && !gSetupWanted &&
         (long)(millis() - nextFetchMs) < -2000;
}
void setPortalCroatian(bool hr) { gPortalHr = hr; }

bool takeTimezone(char *out, size_t n) {
  bool changed = false;
  locked([&] {
    if (gTzChanged) {
      snprintf(out, n, "%s", gTz);
      gTzChanged = false;
      changed = true;
    }
  });
  return changed;
}

} // namespace net
