// Host-side preview: renders the device pages (home, weather for several
// conditions, settings) into PPM files with the same drawing code as the
// device, and prints how long the static and dynamic layers take.
//   gcc -O2 -c lib/qrcodegen/qrcodegen.c -o qrcodegen.o
//   g++ -std=c++17 -O2 -Isrc -Ilib/qrcodegen tools/preview.cpp qrcodegen.o -o preview
//   ./preview out/
#include "Ui.h"
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

static void save(const Canvas &c, const std::string &path) {
  FILE *f = fopen(path.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", c.width(), c.height());
  for (int i = 0; i < c.width() * c.height(); i++) {
    const uint16_t v = c.pixels()[i];
    const uint8_t px[3] = {static_cast<uint8_t>(((v >> 11) & 31) * 255 / 31),
                           static_cast<uint8_t>(((v >> 5) & 63) * 255 / 63),
                           static_cast<uint8_t>((v & 31) * 255 / 31)};
    fwrite(px, 1, 3, f);
  }
  fclose(f);
}

static UiState makeState(Page page, int code, bool day, float temp, float phase, int hour,
                         Lang lang = Lang::Hr) {
  UiState st;
  st.page = page;
  ScreenState &s = st.screen;
  s.lang = lang;
  s.timeValid = true;
  s.hour = hour; s.minute = 9; s.second = 0;
  s.weekday = 4; s.day = 22; s.month = 10;
  s.batteryPercent = code == 63 ? 12 : 78;
  s.charging = code == 3;
  s.wifiConnected = code != -1;
  s.moonPhase = phase;
  s.animTime = 1.3f;
  s.showBack = page != Page::Home;
  if (code >= 0) {
    WeatherData &w = s.weather;
    w.valid = true; w.code = code; w.isDay = day;
    w.temperature = temp; w.feelsLike = temp - 2.4f;
    w.humidity = 72; w.windKmh = 12.4f;
    w.todayMin = temp - 6; w.todayMax = temp + 3;
    snprintf(w.sunrise, sizeof(w.sunrise), "07:12");
    snprintf(w.sunset, sizeof(w.sunset), "17:58");
    const int codes[3] = {61, 2, 0};
    const int precip[3] = {80, 20, 0};
    for (int i = 0; i < 3; i++) {
      DayForecast &d = w.days[i];
      d.code = codes[i];
      d.tempMax = temp + 2 - i * 3;
      d.tempMin = temp - 5 - i;
      d.precipChance = precip[i];
      d.weekday = (5 + i) % 7;
      d.day = 23 + i;
      d.month = 10;
    }
    w.dayCount = 3;
  }
  snprintf(st.info.ssid, sizeof(st.info.ssid), "Home WiFi");
  st.info.rssi = -52;
  snprintf(st.info.ip, sizeof(st.info.ip), "192.168.1.50");
  st.info.brightnessPercent = 78;
  st.info.volumePercent = 60;
  st.info.savedNetworks = 2;
  snprintf(st.info.nets[0].ssid, 33, "My phone");
  snprintf(st.info.nets[1].ssid, 33, "Home WiFi");
  st.info.nets[1].fromCode = true;
  st.info.netCount = 2;
  st.info.setupState = SetupState::Ready;
  snprintf(st.info.apSsid, sizeof(st.info.apSsid), "Sat-4F2A");
  snprintf(st.info.apPassword, sizeof(st.info.apPassword), "k7m2p9qx");
  return st;
}

int main(int argc, char **argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  std::vector<uint16_t> buf(screen::W * screen::H);
  Canvas canvas(buf.data(), screen::W, screen::H);

  struct Case { const char *name; Page page; int code; bool day; float temp; float phase; int hour;
                Lang lang = Lang::Hr; };
  const Case cases[] = {
      {"home", Page::Home, 3, true, 12.6f, 0.3f, 17},
      {"home_night", Page::Home, 0, false, 4.2f, 0.62f, 23},
      {"home_loading", Page::Home, -1, true, 0, 0, 7},
      {"sunny", Page::Weather, 0, true, 17.4f, 0.3f, 14},
      {"clear_night", Page::Weather, 0, false, 4.2f, 0.62f, 23},
      {"partly_night", Page::Weather, 1, false, 8.0f, 0.2f, 21},
      {"overcast", Page::Weather, 3, true, 12.6f, 0.3f, 17},
      {"rain", Page::Weather, 63, true, 11.2f, 0.3f, 16},
      {"snow", Page::Weather, 73, false, -2.6f, 0.9f, 19},
      {"storm", Page::Weather, 95, true, 22.0f, 0.3f, 17},
      {"settings", Page::Settings, 3, true, 12.6f, 0.3f, 17},
      {"settings_end", Page::Settings, 3, true, 12.6f, 0.3f, 17},
      {"forecast", Page::Forecast, 3, true, 12.6f, 0.3f, 17},
      {"brightness", Page::Brightness, 3, true, 12.6f, 0.3f, 17},
      {"wifi", Page::Wifi, 3, true, 12.6f, 0.3f, 17},
      {"wifi_setup", Page::WifiSetup, 3, true, 12.6f, 0.3f, 17},
      {"wifi_setup_lan", Page::WifiSetup, 2, true, 12.6f, 0.3f, 17},
      {"wifi_setup_step2", Page::WifiSetup, 1, true, 12.6f, 0.3f, 17},
      {"location", Page::Location, 3, true, 12.6f, 0.3f, 17},
      {"sound", Page::Sound, 3, true, 12.6f, 0.3f, 17},
      {"home_en", Page::Home, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"weather_en", Page::Weather, 2, true, 12.6f, 0.3f, 17, Lang::En},
      {"forecast_en", Page::Forecast, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"settings_en", Page::Settings, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"settings_end_en", Page::Settings, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"wifi_en", Page::Wifi, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"wifi_setup_en", Page::WifiSetup, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"wifi_setup_step2_en", Page::WifiSetup, 1, true, 12.6f, 0.3f, 17, Lang::En},
      {"wifi_setup_lan_en", Page::WifiSetup, 2, true, 12.6f, 0.3f, 17, Lang::En},
      {"location_en", Page::Location, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"brightness_en", Page::Brightness, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"sound_en", Page::Sound, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"poweroff", Page::PowerOff, 3, true, 12.6f, 0.3f, 17},
      {"apps", Page::Apps, 3, true, 12.6f, 0.3f, 17},
      {"home_timer", Page::Home, 3, true, 12.6f, 0.3f, 17},
      {"timer_idle", Page::Timer, 3, true, 12.6f, 0.3f, 17},
      {"timer_running", Page::Timer, 3, true, 12.6f, 0.3f, 17},
      {"timer_paused", Page::Timer, 3, true, 12.6f, 0.3f, 17},
      {"timer_ringing", Page::Timer, 3, true, 12.6f, 0.3f, 17},
      {"apps_en", Page::Apps, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"home_level", Page::Home, 3, true, 12.6f, 0.3f, 17},
      {"level_tilt", Page::Level, 3, true, 12.6f, 0.3f, 17},
      {"level_ok", Page::Level, 3, true, 12.6f, 0.3f, 17},
      {"level_edge", Page::Level, 3, true, 12.6f, 0.3f, 17},
      {"level_edge_ok", Page::Level, 3, true, 12.6f, 0.3f, 17},
      {"level_vertical", Page::Level, 3, true, 12.6f, 0.3f, 17},
      {"home_level_en", Page::Home, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"level_tilt_en", Page::Level, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"level_ok_en", Page::Level, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"level_edge_en", Page::Level, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"home_timer_en", Page::Home, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"timer_idle_en", Page::Timer, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"timer_running_en", Page::Timer, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"timer_ringing_en", Page::Timer, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"poweroff_en", Page::PowerOff, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"night_en", Page::Weather, 0, false, 4.2f, 0.62f, 23, Lang::En},
      {"home_assistant", Page::Home, 3, true, 12.6f, 0.3f, 17},
      {"assistant_ready", Page::Assistant, 3, true, 12.6f, 0.3f, 17},
      {"assistant_listening", Page::Assistant, 3, true, 12.6f, 0.3f, 17},
      {"assistant_thinking", Page::Assistant, 3, true, 12.6f, 0.3f, 17},
      {"assistant_answer", Page::Assistant, 3, true, 12.6f, 0.3f, 17},
      {"assistant_long", Page::Assistant, 3, true, 12.6f, 0.3f, 17},
      {"assistant_setup", Page::Assistant, 3, true, 12.6f, 0.3f, 17},
      {"assistant_error", Page::Assistant, 3, true, 12.6f, 0.3f, 17},
      {"home_assistant_en", Page::Home, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"assistant_ready_en", Page::Assistant, 3, true, 12.6f, 0.3f, 17, Lang::En},
      {"assistant_answer_en", Page::Assistant, 3, true, 12.6f, 0.3f, 17, Lang::En},
  };
  for (const Case &cs : cases) {
    UiState st = makeState(cs.page, cs.code, cs.day, cs.temp, cs.phase, cs.hour, cs.lang);
    const std::string name = cs.name;
    if (name.rfind("wifi_setup_lan", 0) == 0) st.info.setupOverLan = true;
    if (name.rfind("wifi_setup_step2", 0) == 0) st.info.apClients = 1;
    if (name.rfind("wifi_setup", 0) == 0) {
      snprintf(st.info.apSsid, sizeof(st.info.apSsid), "Clock-2F88");
    }
    // README screenshots: neutral example data, nothing from a real setup.
    if (name.size() > 3 && name.compare(name.size() - 3, 3, "_en") == 0) {
      st.screen.city = "Zagreb";
      snprintf(st.info.ssid, sizeof(st.info.ssid), "Network");
      snprintf(st.info.ip, sizeof(st.info.ip), "192.168.1.50");
    }
    if (name == "sound") st.info.chimeOn = true;
    if (name.rfind("apps", 0) == 0 || name.rfind("home_timer", 0) == 0) st.app = HomeApp::Timer;
    if (name.rfind("home_timer", 0) == 0 || name.rfind("timer_running", 0) == 0 ||
        name.rfind("timer_paused", 0) == 0) {
      st.timer.state = name.rfind("timer_paused", 0) == 0 ? TimerState::Paused
                                                         : TimerState::Running;
      st.timer.totalSec = 8 * 60;
      st.timer.remainingSec = 7 * 60 + 42;
    }
    if (name.rfind("timer_ringing", 0) == 0) st.timer.state = TimerState::Ringing;
    if (name.rfind("home_level", 0) == 0) st.app = HomeApp::Level;
    if (name.rfind("apps", 0) == 0) st.app = HomeApp::Level;
    if (name.rfind("level_tilt", 0) == 0) { st.level.x = 3.4f; st.level.y = -2.1f; }
    if (name.rfind("level_ok", 0) == 0) { st.level.x = 0.2f; st.level.y = -0.1f; st.level.calibrated = true; }
    if (name.rfind("level_edge", 0) == 0) {
      st.level.edge = true;
      st.level.angle = name.rfind("level_edge_ok", 0) == 0 ? 0.3f : -4.6f;
    }
    if (name == "level_vertical") { st.level.edge = true; st.level.vertical = true; st.level.angle = 2.8f; }
    if (name == "wifi_en") {
      snprintf(st.info.nets[0].ssid, 33, "My phone hotspot");
      snprintf(st.info.nets[1].ssid, 33, "Network");
      st.info.nets[1].fromCode = false;
    }
    if (name.rfind("home_assistant", 0) == 0) st.app = HomeApp::Assistant;
    if (name.rfind("assistant", 0) == 0) {
      AssistantInfo &ai = st.assistant;
      if (name == "assistant_listening") {
        ai.phase = AssistantPhase::Listening; ai.recordMs = 4200; ai.level = 0.8f;
      }
      if (name == "assistant_thinking") { ai.phase = AssistantPhase::Thinking; ai.turns = 1; }
      if (name == "assistant_answer") {
        ai.phase = AssistantPhase::Answer; ai.turns = 2;
        ai.answer = "Triglav je visok 2864 m. To je najviši vrh Slovenije i Julijskih Alpa, "
                    "a nalazi se u Triglavskom nacionalnom parku.";
      }
      if (name == "assistant_answer_en") {
        ai.phase = AssistantPhase::Answer; ai.turns = 1;
        ai.answer = "Soft: 6 minutes. Medium: 8 minutes. Hard: 10 to 11 minutes. Start timing "
                    "when the water boils, then cool the egg in cold water.";
      }
      if (name == "assistant_long") {
        ai.phase = AssistantPhase::Answer; ai.turns = 3;
        ai.answer = "Za palačinke ti treba 2 jaja, 250 ml mlijeka, 150 g brašna i prstohvat soli. "
                    "Sve izmiješaj u glatko tijesto i ostavi ga 15 minuta da odstoji. Zagrij "
                    "tavu, lagano je namaži uljem i ulij tanak sloj tijesta. Peci oko minutu sa "
                    "svake strane, dok ne dobije zlatnu boju. Poslužuj s marmeladom, nutelom ili "
                    "orasima, a možeš dodati i malo šećera u prahu po vrhu.";
      }
      if (name == "assistant_setup") { ai.phase = AssistantPhase::Error; ai.error = AssistantError::NotSetUp; }
      if (name == "assistant_error") {
        ai.phase = AssistantPhase::Error; ai.error = AssistantError::Server; ai.status = 502; ai.turns = 1;
      }
    }
    if (name.rfind("settings_end", 0) == 0) {
      st.scroll = ui::kSettingsMaxScroll;
    }
    ui::draw(canvas, st);
    if (cs.page == Page::Settings) {
      // The rows live in their own tall layer, as on the device.
      std::vector<uint16_t> list(screen::W * ui::kSettingsContentH);
      Canvas lc(list.data(), screen::W, ui::kSettingsContentH);
      ui::drawSettingsList(lc, st);
      ui::composeSettings(canvas, list.data(), st.scroll);
    }
    save(canvas, dir + "/" + cs.name + ".ppm");
  }

  // Timing: static layer vs. one dynamic frame (restore + redraw).
  using clk = std::chrono::steady_clock;
  UiState st = makeState(Page::Weather, 63, true, 11.2f, 0.3f, 16);
  const int N = 50;
  auto t0 = clk::now();
  for (int i = 0; i < N; i++) ui::drawStatic(canvas, st);
  auto t1 = clk::now();
  std::vector<uint16_t> base = buf;
  Rect rects[4];
  for (int i = 0; i < N; i++) {
    const int n = ui::dynamicRects(canvas, st, rects);
    for (int r = 0; r < n; r++)
      for (int y = rects[r].y; y < rects[r].y + rects[r].h; y++)
        memcpy(&buf[y * screen::W + rects[r].x], &base[y * screen::W + rects[r].x],
               rects[r].w * 2);
    st.screen.animTime += 0.1f;
    ui::drawDynamic(canvas, st);
  }
  auto t2 = clk::now();
  printf("static %.2f ms, dynamic %.2f ms (host)\n",
         std::chrono::duration<double, std::milli>(t1 - t0).count() / N,
         std::chrono::duration<double, std::milli>(t2 - t1).count() / N);
  return 0;
}
