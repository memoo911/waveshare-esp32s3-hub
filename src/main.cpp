// Clock and weather for the Waveshare ESP32-S3-Touch-AMOLED-1.8.
//
// - Home screen with Weather and Settings tiles; tap to open, swipe right or
//   tap "‹" to go back.
// - Settings: WiFi and location set up from a phone via QR code, brightness,
//   sound, language (English / Croatian).
// - Time from NTP, time zone detected from the network; weather from
//   Open-Meteo (no API key). Networking lives in Net.cpp.
// - Animated icon for the current conditions; at night the moon shows its
//   real phase.
// - BOOT button: refresh the weather now (or wake the screen). PWR short
//   press: screen off/on. The screen sleeps after 1/2/5 minutes idle (or
//   never, Settings -> Display); a touch or picking the board up wakes it.
// - Battery care: on USB, charging pauses at 100 % until 90 %.
// - Sound: volume slider and an optional hourly chime (Audio.cpp).
//
// Rendering uses two PSRAM layers: a static base (redrawn only when its
// content changes, see ui::staticKey) and the frame, where only the animated
// regions are restored from the base and redrawn each tick. Only pixels that
// changed are sent to the panel.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>
#include <XPowersLib.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <time.h>

#include "Audio.h"
#include "Config.h"
#include "Gesture.h"
#include "Imu.h"
#include "Net.h"
#include "Touch.h"
#include "Ui.h"

namespace {

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS_PIN, LCD_SCLK_PIN, LCD_SDIO0_PIN,
                                             LCD_SDIO1_PIN, LCD_SDIO2_PIN, LCD_SDIO3_PIN);
#if defined(WAVESHARE_PANEL_CO5300)
// V2 board. Built against GFX Library 1.5.9 (see platformio.ini), whose
// constructor still has an `ips` flag after the rotation argument.
Arduino_CO5300 *panel = new Arduino_CO5300(bus, GFX_NOT_DEFINED, 0, false, SCREEN_WIDTH_PX,
                                           SCREEN_HEIGHT_PX, LCD_PANEL_COL_OFFSET, 0, 0, 0);
#else
// V1 board. The SH8601 driver only exists in GFX Library 1.6+, which needs
// Arduino-ESP32 core 3.x.
Arduino_SH8601 *panel = new Arduino_SH8601(bus, GFX_NOT_DEFINED, 0, SCREEN_WIDTH_PX,
                                           SCREEN_HEIGHT_PX, LCD_PANEL_COL_OFFSET, 0, 0, 0);
#endif

XPowersPMU pmu;
bool pmuReady = false;
Imu imu;
unsigned long lastLiftPollMs = 0;

// Static layer, cached per page so switching back is instant.
uint16_t *bases[kPageCount] = {};
uint32_t baseKeys[kPageCount] = {};
bool baseValid[kPageCount] = {};
uint16_t *frame = nullptr;     // base + animated regions
uint16_t *shown = nullptr;     // what the panel currently shows
uint16_t *bandBuf = nullptr;   // contiguous copy of one dirty band
constexpr int kFlushBand = 16; // rows per transfer; even, divides 448
bool panelHasFrame = false;
// Settings: the scrolling rows, drawn once into a tall layer (see Ui.h).
uint16_t *settingsList = nullptr;
uint32_t settingsListKey = 0;
bool settingsListValid = false;
int settingsScroll = 0;
int shownScroll = -1;
// Finger dragging the settings list.
bool prevFingerDown = false;
bool scrollTracking = false;
bool touchScrolled = false; // this touch scrolled: it is not a tap or swipe
int dragStartX = 0, dragStartY = 0, dragStartScroll = 0;


volatile bool screenOn = true; // read by the touch task too
uint8_t currentBrightness = 0;
// Last touch or button press; the screen sleeps SCREEN_TIMEOUT_MS after it.
unsigned long lastActivityMs = 0;
// Redraw the whole screen on the next frame (after waking up).
bool forceFullFrame = false;
// The touch that woke the screen: ignored until the finger is lifted, so it
// doesn't also open a page.
bool wakeTouch = false;
unsigned long wakeReleaseMs = 0;

// Battery, read every few seconds (also while the screen is off).
int batteryPercent = -1;
bool batteryCharging = false;
bool chargePaused = false;
bool usbPowered = true; // until the first battery reading
// After a wake-up by touch or BOOT, stay awake briefly so the touch task and
// button polling see what happened.
unsigned long stayAwakeUntilMs = 0;

TouchInput touch;
GestureDetector gestures;
// The touch task samples the panel at 100 Hz, independent of drawing, and
// hands finished gestures to the UI loop through this queue.
QueueHandle_t gestureQueue = nullptr;
// Latest raw finger position, for the brightness slider (guarded by touchLock).
portMUX_TYPE touchLock = portMUX_INITIALIZER_UNLOCKED;
// The touch chip and the PMU share one I2C bus, used from two tasks.
SemaphoreHandle_t i2cMutex = nullptr;

struct I2cLock {
  I2cLock() { if (i2cMutex) xSemaphoreTake(i2cMutex, portMAX_DELAY); }
  ~I2cLock() { if (i2cMutex) xSemaphoreGive(i2cMutex); }
};
volatile bool fingerDown = false;
int fingerX = 0, fingerY = 0;
Page page = Page::Home;
Preferences prefs;
// User brightness in percent (5..100), saved in NVS.
int brightnessSetting = 80;
bool brightnessDirty = false;
unsigned long brightnessChangedMs = 0;
Lang language = DEFAULT_LANGUAGE;
// Volume in percent (0..100) and the hourly chime, saved in NVS.
int volumeSetting = VOLUME_DEFAULT_PERCENT;
bool volumeDirty = false;
unsigned long volumeChangedMs = 0;
bool volumeDragging = false; // finger on the volume slider (preview on release)
bool chimeOn = false;
// Screen-off time: index into ui::kScreenOffMinutes, saved in NVS.
int screenOffIndex = SCREEN_TIMEOUT_DEFAULT;
// Night dimming (NIGHT_START_HOUR..NIGHT_END_HOUR), saved in NVS.
bool nightDim = true;
// WiFi page: a saved network waiting for a second tap on its "×".
int pendingDelete = -1;
unsigned long pendingDeleteMs = 0;
char pendingDeleteSsid[33] = "";
// When BOOT last asked for a refresh (the notice stays up while it runs).
unsigned long refreshStartMs = 0;

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
void setBrightness(uint8_t level) {
  if (level == currentBrightness) return;
  panel->setBrightness(level);
  currentBrightness = level;
}

// Send what changed in rows [yFrom, yTo) since the last frame, one 16-row
// band at a time. The AMOLED controller ignores windows that start on an odd
// row/column or have an odd height/width, so every transfer is aligned to 2
// pixels: bands start on a multiple of 16 rows, x/width are rounded to even.
int flushedBands = 0;

void flushFrame(int yFrom = 0, int yTo = SCREEN_HEIGHT_PX) {
  const int W = SCREEN_WIDTH_PX, H = SCREEN_HEIGHT_PX;
  if (!panelHasFrame) {
    panel->draw16bitRGBBitmap(0, 0, frame, W, H);
    memcpy(shown, frame, W * H * sizeof(uint16_t));
    panelHasFrame = true;
    return;
  }
  const int bandStart = max(0, yFrom) / kFlushBand * kFlushBand;
  for (int y0 = bandStart; y0 < min(H, yTo); y0 += kFlushBand) {
    const int h = min(kFlushBand, H - y0);
    int minX = W, maxX = -1;
    for (int y = y0; y < y0 + h; y++) {
      const uint16_t *a = frame + y * W;
      const uint16_t *b = shown + y * W;
      int l = 0;
      while (l < W && a[l] == b[l]) l++;
      if (l == W) continue;
      int r = W - 1;
      while (r > l && a[r] == b[r]) r--;
      minX = min(minX, l);
      maxX = max(maxX, r);
    }
    if (maxX < 0) continue;
    minX &= ~1;
    maxX |= 1;
    if (maxX >= W) maxX = W - 1;
    const int w = maxX - minX + 1;
    for (int y = 0; y < h; y++) {
      memcpy(bandBuf + y * w, frame + (y0 + y) * W + minX, w * sizeof(uint16_t));
      memcpy(shown + (y0 + y) * W + minX, frame + (y0 + y) * W + minX,
             w * sizeof(uint16_t));
    }
    panel->draw16bitRGBBitmap(minX, y0, bandBuf, w, h);
    flushedBands++;
  }
}

// Copy a rectangle from the static layer into the frame.
void restoreRect(const uint16_t *base, const Rect &r) {
  const int W = SCREEN_WIDTH_PX, H = SCREEN_HEIGHT_PX;
  const int x0 = max(0, r.x), x1 = min(W, r.x + r.w);
  const int y0 = max(0, r.y), y1 = min(H, r.y + r.h);
  if (x1 <= x0) return;
  for (int y = y0; y < y1; y++) {
    memcpy(frame + y * W + x0, base + y * W + x0, (x1 - x0) * sizeof(uint16_t));
  }
}

void setScreenOn(bool on) {
  if (on == screenOn) return;
  screenOn = on;
  lastActivityMs = millis();
  Serial.printf("[UI] screen %s\n", on ? "on" : "off");
  if (on) {
    // Full speed and WiFi back first, so the screen reacts at once.
    setCpuFrequencyMhz(240);
    net::setLowPower(false);
    forceFullFrame = true;
    imu.disarm();
    panel->displayOn();
    // The panel may come out of sleep at its own brightness: send ours again.
    panel->setBrightness(currentBrightness);
  } else {
    panel->displayOff();
    // Screen off: slower CPU, WiFi only for the periodic refresh.
    net::setLowPower(true);
    setCpuFrequencyMhz(CPU_MHZ_SCREEN_OFF);
  }
}

void openPage(Page p) {
  if (p == page) return;
  if (p == Page::Settings && page == Page::Home) settingsScroll = 0;
  // Pages that need the network task to do something while they're open.
  if (page == Page::WifiSetup) net::setSetupWanted(false);
  if (page == Page::Location) net::setWebWanted(false);
  page = p;
  if (p == Page::WifiSetup) net::setSetupWanted(true);
  if (p == Page::Location) net::setWebWanted(true);
  static const char *names[] = {"home",       "weather", "forecast",   "settings",
                                "brightness", "wifi",    "wifi setup", "location",
                                "sound",      "turn off"};
  Serial.printf("[UI] page %s\n", names[static_cast<int>(p)]);
}

void toggleLanguage() {
  language = language == Lang::En ? Lang::Hr : Lang::En;
  prefs.putUChar("lang", static_cast<uint8_t>(language));
  net::setPortalCroatian(language == Lang::Hr);
  Serial.printf("[UI] language %s\n", language == Lang::Hr ? "hr" : "en");
}

// Settings -> Turn off -> red button: save what's pending and switch the board off through
// the PMU (like holding PWR). PWR turns it on again.
void saveSettingsNow();

void turnOff() {
  Serial.println("[PMU] turning off");
  saveSettingsNow();
  setBrightness(0);
  panel->displayOff();
  Serial.flush();
  delay(200);
  {
    I2cLock lock;
    pmu.shutdown();
  }
  // Normally the power is gone by now.
  delay(3000);
  Serial.println("[PMU] still running after shutdown");
  panel->displayOn();
  forceFullFrame = true;
  openPage(Page::Settings);
}

// ---------------------------------------------------------------------------
// Buttons and touch
// ---------------------------------------------------------------------------
bool bootWasDown = false;
unsigned long lastPmuPollMs = 0;

void requestRefresh() {
  Serial.println("[UI] refresh weather");
  refreshStartMs = millis();
  net::requestRefresh();
}

void pollButtons() {
  const bool bootDown = digitalRead(BUTTON_BOOT_PIN) == LOW;
  if (bootDown && !bootWasDown) {
    Serial.println("[UI] BOOT");
    lastActivityMs = millis();
    if (!screenOn) {
      setScreenOn(true);
    } else {
      requestRefresh();
    }
  }
  bootWasDown = bootDown;

  if (pmuReady && millis() - lastPmuPollMs > 50) {
    lastPmuPollMs = millis();
    bool pressed;
    {
      I2cLock lock;
      pmu.getIrqStatus();
      pressed = pmu.isPekeyShortPressIrq();
      pmu.clearIrqStatus();
    }
    if (pressed) setScreenOn(!screenOn);
  }
}

// Turn the screen off after a while without touches or button presses, and
// watch for the board being picked up while it's off.
void sleepIfIdle() {
  if (!screenOn) return;
  const int minutes = ui::kScreenOffMinutes[screenOffIndex];
  if (minutes == 0 || page == Page::WifiSetup || page == Page::Location) {
    lastActivityMs = millis();
    return;
  }
  if (millis() - lastActivityMs > minutes * 60000UL) {
    Serial.println("[UI] idle, screen off");
    setScreenOn(false);
    // Only after going dark by itself: PWR means "keep it off".
    if (imu.ready()) {
      I2cLock lock;
      imu.arm();
    }
  }
}

void wakeOnLift() {
  if (screenOn || !imu.ready() || millis() - lastLiftPollMs < LIFT_POLL_MS) return;
  lastLiftPollMs = millis();
  bool lifted;
  {
    I2cLock lock;
    lifted = imu.lifted();
  }
  if (lifted) setScreenOn(true);
}

void toggleNightDim() {
  nightDim = !nightDim;
  prefs.putBool("nightdim", nightDim);
  Serial.printf("[UI] night dimming %s\n", nightDim ? "on" : "off");
}

// Screen off, on battery, WiFi resting, nothing playing: light-sleep for a
// moment. A touch (interrupt pin) or BOOT wakes the chip at once; otherwise
// the timer does, so the clock, chime, refresh schedule, PWR button and
// pick-up check keep running between naps.
void lightSleepIfIdle() {
  if (!LIGHT_SLEEP || screenOn || usbPowered || !net::radioAsleep() || audio::busy()) return;
  if ((long)(millis() - stayAwakeUntilMs) < 0) return;
  if (fingerDown) return;
  const bool touchIdle = digitalRead(TOUCH_INT_PIN) == HIGH;
  const bool bootIdle = digitalRead(BUTTON_BOOT_PIN) == HIGH;
  if (touchIdle) gpio_wakeup_enable(static_cast<gpio_num_t>(TOUCH_INT_PIN), GPIO_INTR_LOW_LEVEL);
  if (bootIdle) gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_BOOT_PIN), GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_MS * 1000ULL);
  esp_light_sleep_start();
  if (touchIdle) gpio_wakeup_disable(static_cast<gpio_num_t>(TOUCH_INT_PIN));
  if (bootIdle) gpio_wakeup_disable(static_cast<gpio_num_t>(BUTTON_BOOT_PIN));
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
    stayAwakeUntilMs = millis() + 200;
  }
}

void nextScreenOff() {
  screenOffIndex = (screenOffIndex + 1) % ui::kScreenOffChoices;
  prefs.putUChar("scroff", static_cast<uint8_t>(screenOffIndex));
  const int m = ui::kScreenOffMinutes[screenOffIndex];
  if (m) Serial.printf("[UI] screen off after %d min\n", m);
  else Serial.println("[UI] screen off never");
}

// Read the battery every few seconds and, on USB, pause charging at full
// until it has dropped to BATTERY_RESUME_PERCENT.
void pollBattery() {
  static unsigned long lastMs = 0;
  if (!pmuReady || (lastMs && millis() - lastMs < 5000)) return;
  lastMs = millis();
  bool shutDown = false;
  {
    I2cLock lock;
    const bool present = pmu.isBatteryConnect();
    batteryPercent = present ? pmu.getBatteryPercent() : -1;
    const bool usb = pmu.isVbusIn();
    usbPowered = usb;
    // Battery almost empty and nothing charging it: switch off (two readings
    // in a row, so one odd value right after start can't do it).
    static int lowReadings = 0;
    if (present && !usb && batteryPercent >= 0 && batteryPercent <= BATTERY_SHUTDOWN_PERCENT) {
      lowReadings++;
    } else {
      lowReadings = 0;
    }
    shutDown = lowReadings >= 2;
    // Power log, to see how fast the battery drains.
    static unsigned long lastLogMs = 0;
    if (lastLogMs == 0 || millis() - lastLogMs >= POWER_LOG_MS) {
      lastLogMs = millis();
      Serial.printf("[Power] battery %d%% %dmV usb=%d charging=%d screen=%s cpu=%luMHz wifi=%s\n",
                    batteryPercent, present ? pmu.getBattVoltage() : 0, usb ? 1 : 0,
                    pmu.isCharging() ? 1 : 0, screenOn ? "on" : "off",
                    static_cast<unsigned long>(getCpuFrequencyMhz()),
                    WiFi.status() == WL_CONNECTED ? "on" : "off");
    }
    if (BATTERY_CARE && present && batteryPercent >= 0) {
      if (!chargePaused && batteryPercent >= BATTERY_FULL_PERCENT) {
        pmu.disableCellbatteryCharge();
        chargePaused = true;
        Serial.printf("[PMU] battery %d%%, charging paused\n", batteryPercent);
      } else if (chargePaused && batteryPercent <= BATTERY_RESUME_PERCENT) {
        pmu.enableCellbatteryCharge();
        chargePaused = false;
        Serial.printf("[PMU] battery %d%%, charging resumed\n", batteryPercent);
      }
    }
    batteryCharging = pmu.isCharging();
  }
  if (shutDown) {
    Serial.printf("[PMU] battery %d%% on battery power, turning off\n", batteryPercent);
    turnOff();
  }
}

void setBrightnessSetting(int percent) {
  percent = constrain(percent, ui::kBrightnessMin, ui::kBrightnessMax);
  if (percent == brightnessSetting) return;
  brightnessSetting = percent;
  brightnessDirty = true;
  brightnessChangedMs = millis();
}

void setVolumeSetting(int percent) {
  percent = constrain(percent, 0, 100);
  if (percent == volumeSetting) return;
  volumeSetting = percent;
  audio::setVolume(percent);
  volumeDirty = true;
  volumeChangedMs = millis();
}

void toggleChime() {
  chimeOn = !chimeOn;
  prefs.putBool("chime", chimeOn);
  Serial.printf("[UI] hourly chime %s\n", chimeOn ? "on" : "off");
}

// Hourly chime on the full hour, if switched on and not at night.
void chimeIfHour() {
  static int lastHour = -1;
  const time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  if (t.tm_year < 120) return; // clock not set yet
  if (t.tm_min != 0) {
    lastHour = -1;
    return;
  }
  if (t.tm_hour == lastHour) return;
  lastHour = t.tm_hour;
  const bool night = t.tm_hour >= NIGHT_START_HOUR || t.tm_hour < NIGHT_END_HOUR;
  if (!chimeOn || night) return;
  Serial.printf("[Audio] chime %02d:00\n", t.tm_hour);
  audio::playChime();
}

void saveSettingsNow() {
  if (brightnessDirty) prefs.putUChar("bright", static_cast<uint8_t>(brightnessSetting));
  if (volumeDirty) prefs.putUChar("vol", static_cast<uint8_t>(volumeSetting));
  brightnessDirty = volumeDirty = false;
}

// Write the brightness to flash once the finger has rested for a moment.
void saveBrightnessIfIdle() {
  if (brightnessDirty && millis() - brightnessChangedMs > 1500) {
    prefs.putUChar("bright", static_cast<uint8_t>(brightnessSetting));
    brightnessDirty = false;
    Serial.printf("[UI] brightness %d%% saved\n", brightnessSetting);
  }
  if (volumeDirty && millis() - volumeChangedMs > 1500) {
    prefs.putUChar("vol", static_cast<uint8_t>(volumeSetting));
    volumeDirty = false;
    Serial.printf("[UI] volume %d%% saved\n", volumeSetting);
  }
}

void touchTask(void *) {
  for (;;) {
    int x = 0, y = 0;
    bool down;
    {
      I2cLock lock;
      down = touch.read(x, y);
    }
    portENTER_CRITICAL(&touchLock);
    fingerDown = down;
    if (down) {
      fingerX = x;
      fingerY = y;
    }
    portEXIT_CRITICAL(&touchLock);
    const Gesture g = gestures.update(down, x, y, millis());
    if (g.type != GestureType::None) xQueueSend(gestureQueue, &g, 0);
    // Screen off: a slower check is still quick enough to wake on a touch.
    vTaskDelay(pdMS_TO_TICKS(screenOn ? TOUCH_POLL_MS : 50));
  }
}

void handleTouch(const UiState &st) {
  // The brightness slider follows the finger directly.
  portENTER_CRITICAL(&touchLock);
  const bool down = fingerDown;
  const int fx = fingerX, fy = fingerY;
  portEXIT_CRITICAL(&touchLock);

  // Touch while the screen is off: wake up, and swallow this touch.
  if (down) lastActivityMs = millis();
  if (!screenOn && down) {
    Serial.println("[Touch] wake");
    setScreenOn(true);
    wakeTouch = true;
    wakeReleaseMs = 0;
  }
  if (wakeTouch) {
    // The gesture for the waking touch arrives shortly after the release.
    if (!down && !wakeReleaseMs) wakeReleaseMs = millis();
    if (down) wakeReleaseMs = 0;
    if (wakeReleaseMs && millis() - wakeReleaseMs > 300) wakeTouch = false;
  }
  // Settings list follows a vertical drag.
  if (screenOn && !wakeTouch && page == Page::Settings) {
    if (down && !prevFingerDown) {
      dragStartX = fx;
      dragStartY = fy;
      dragStartScroll = settingsScroll;
      scrollTracking = fy >= ui::kListTop;
      touchScrolled = false;
    }
    if (down && scrollTracking) {
      const int dy = fy - dragStartY, dx = fx - dragStartX;
      if (!touchScrolled && abs(dy) > 10 && abs(dy) > abs(dx)) touchScrolled = true;
      if (touchScrolled) {
        settingsScroll = constrain(dragStartScroll - dy, 0, ui::kSettingsMaxScroll);
      }
    }
  }
  prevFingerDown = down;

  int value = 0;
  if (screenOn && !wakeTouch && down && ui::sliderDrag(st, fx, fy, value)) {
    if (page == Page::Sound) {
      setVolumeSetting(value);
      volumeDragging = true;
    } else {
      setBrightnessSetting(value);
    }
  }
  // Let go of the volume slider: a short note at the new volume.
  if (!down && volumeDragging) {
    volumeDragging = false;
    audio::playBlip();
  }

  Gesture g;
  while (gestureQueue && xQueueReceive(gestureQueue, &g, 0) == pdTRUE) {
    if (!screenOn || wakeTouch) continue;
    // A touch that scrolled the settings list is not a tap or swipe.
    if (touchScrolled) {
      touchScrolled = false;
      continue;
    }
    // A drag that started on the slider is not a swipe or tap.
    if (ui::sliderDrag(st, g.x, g.y, value)) continue;
    switch (g.type) {
    case GestureType::Tap: {
      Serial.printf("[Touch] tap %d,%d\n", g.x, g.y);
      const int del = ui::wifiDeleteAt(st, g.x, g.y);
      if (del >= 0) {
        // First tap asks, second tap on the same row deletes.
        if (del == pendingDelete && millis() - pendingDeleteMs < 4000) {
          Serial.printf("[UI] forget network \"%s\"\n", pendingDeleteSsid);
          net::forget(pendingDeleteSsid);
          pendingDelete = -1;
        } else {
          pendingDelete = del;
          pendingDeleteMs = millis();
          snprintf(pendingDeleteSsid, sizeof(pendingDeleteSsid), "%s",
                   st.info.nets[del].ssid);
        }
        break;
      }
      pendingDelete = -1;
      if (ui::tapsPowerOff(st, g.x, g.y)) {
        turnOff();
        break;
      }
      if (ui::tapsLanguage(st, g.x, g.y)) toggleLanguage();
      else if (ui::tapsChime(st, g.x, g.y)) toggleChime();
      else if (ui::tapsScreenOff(st, g.x, g.y)) nextScreenOff();
      else if (ui::tapsNightDim(st, g.x, g.y)) toggleNightDim();
      else openPage(ui::onTap(st, g.x, g.y));
      break;
    }
    case GestureType::SwipeRight:
      Serial.println("[Touch] swipe right");
      openPage(ui::onSwipeRight(st));
      break;
    case GestureType::SwipeLeft:
      Serial.println("[Touch] swipe left");
      openPage(ui::onSwipeLeft(st));
      break;
    case GestureType::SwipeUp: Serial.println("[Touch] swipe up"); break;
    case GestureType::SwipeDown: Serial.println("[Touch] swipe down"); break;
    case GestureType::None: break;
    }
  }
}

void initPmu() {
  Wire.begin(BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
  pmuReady = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, BOARD_I2C_SDA_PIN, BOARD_I2C_SCL_PIN);
  if (!pmuReady) {
    Serial.println("[PMU] AXP2101 not found");
    return;
  }
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.clearIrqStatus();
  pmu.setChargeTargetVoltage(3); // 4.2 V, matches the Li-Po
  pmu.setSysPowerDownVoltage(2600);
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  // Charging may have been left paused before a restart; battery care
  // decides again from the current level.
  pmu.enableCellbatteryCharge();
  Serial.printf("[PMU] batt=%dmV pct=%d charging=%d\n", pmu.getBattVoltage(),
                pmu.getBatteryPercent(), pmu.isCharging() ? 1 : 0);
}

void logI2cDevices() {
  Serial.print("[I2C] devices:");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a);
  }
  Serial.println();
}

// Screen text for the WiFi state while there's no weather to show.
const char *wifiMessage(const WifiState &w, char *buf, size_t n) {
  const Lang L = language;
  char shortSsid[18];
  snprintf(shortSsid, sizeof(shortSsid), "%s", w.ssid); // keep long names on screen
  switch (w.code) {
  case WifiCode::ConnectingTo: snprintf(buf, n, i18n::tr(L, Str::ConnectingTo), shortSsid); break;
  case WifiCode::CantFind: snprintf(buf, n, i18n::tr(L, Str::CantFind), shortSsid); break;
  case WifiCode::WrongPassword: snprintf(buf, n, i18n::tr(L, Str::WrongPassword), shortSsid); break;
  case WifiCode::Unavailable: snprintf(buf, n, i18n::tr(L, Str::WifiUnavailable), shortSsid); break;
  case WifiCode::NoNetworks: snprintf(buf, n, "%s", i18n::tr(L, Str::SetUpWifi)); break;
  default: snprintf(buf, n, "%s", i18n::tr(L, Str::ConnectingWifi)); break;
  }
  return buf;
}

// Gather everything the UI shows into one snapshot.
void buildState(UiState &st, unsigned long now) {
  ScreenState &s = st.screen;
  st.page = page;
  s.lang = language;
  static Place place;
  place = net::place();
  s.city = place.name;
  s.showBack = page != Page::Home;

  struct tm tm;
  if (getLocalTime(&tm, 0) && tm.tm_year > 120) {
    s.timeValid = true;
    s.hour = tm.tm_hour;
    s.minute = tm.tm_min;
    s.second = tm.tm_sec;
    s.weekday = tm.tm_wday;
    s.day = tm.tm_mday;
    s.month = tm.tm_mon + 1;
  }

  bool failed = false;
  unsigned long goodAt = 0;
  net::weather(s.weather, failed, goodAt);
  // WiFi switched off on purpose while the screen was off counts as
  // connected, so no warning flashes while it reconnects.
  s.wifiConnected = WiFi.status() == WL_CONNECTED || net::radioResting();
  s.weatherStale = s.weather.valid && (failed || now - goodAt > WEATHER_STALE_MS);
  if (!s.weather.valid) {
    static char status[64];
    s.message = !s.wifiConnected ? wifiMessage(net::wifiState(), status, sizeof(status))
                : failed         ? i18n::tr(language, Str::ForecastUnavailable)
                                 : i18n::tr(language, Str::LoadingForecast);
  }
  // Keep the notice up at least briefly so a quick refresh is still visible.
  if (refreshStartMs && (net::refreshing() || now - refreshStartMs < 900)) {
    s.toast = i18n::tr(language, Str::Refreshing);
  }

  s.batteryPercent = batteryPercent;
  s.charging = batteryCharging;

  s.moonPhase = icons::moonPhase(static_cast<double>(time(nullptr)));
  s.animTime = now / 1000.0f;

  DeviceInfo &d = st.info;
  if (s.wifiConnected) {
    snprintf(d.ssid, sizeof(d.ssid), "%s", WiFi.SSID().c_str());
    d.rssi = WiFi.RSSI();
    snprintf(d.ip, sizeof(d.ip), "%s", WiFi.localIP().toString().c_str());
  }
  d.brightnessPercent = brightnessSetting;
  d.volumePercent = volumeSetting;
  d.chimeOn = chimeOn;
  d.screenOff = screenOffIndex;
  d.nightDim = nightDim;
  d.savedNetworks = net::savedNetworkCount();
  d.setupOverLan = net::setupOverLan();
  d.apClients = net::setupClients();
  net::NetEntry list[DeviceInfo::kMaxNets];
  d.netCount = net::networks(list, DeviceInfo::kMaxNets);
  for (int i = 0; i < d.netCount; i++) {
    snprintf(d.nets[i].ssid, sizeof(d.nets[i].ssid), "%s", list[i].ssid);
    d.nets[i].fromCode = list[i].fromCode;
  }
  if (pendingDelete >= 0 && (page != Page::Wifi || millis() - pendingDeleteMs > 4000)) {
    pendingDelete = -1;
  }
  d.pendingDelete = pendingDelete;
  d.setupState = net::setupState();
  net::setupCredentials(d.apSsid, sizeof(d.apSsid), d.apPassword, sizeof(d.apPassword),
                        d.savedSsid, sizeof(d.savedSsid));
}

// Leave the WiFi setup page once setup is over (saved and connected, or
// timed out).
void followSetup(const UiState &st) {
  if (page != Page::WifiSetup) return;
  static unsigned long savedAt = 0;
  if (st.info.setupState == SetupState::Saved) {
    if (!savedAt) savedAt = millis();
    // Show "Saved" for a moment, then go back once the board is online.
    const unsigned long shown = millis() - savedAt;
    if ((st.screen.wifiConnected && shown > 2000) || shown > 20000) {
      savedAt = 0;
      openPage(Page::Wifi);
    }
  } else if (!net::setupActive()) {
    savedAt = 0;
    openPage(Page::Wifi);
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("[Boot] weather clock");
  pinMode(BUTTON_BOOT_PIN, INPUT_PULLUP);
  initPmu();
  imu.begin();

  prefs.begin("weather", false);
  brightnessSetting = prefs.getUChar("bright", BRIGHTNESS_DEFAULT_PERCENT);
  volumeSetting = prefs.getUChar("vol", VOLUME_DEFAULT_PERCENT);
  chimeOn = prefs.getBool("chime", false);
  nightDim = prefs.getBool("nightdim", true);
  screenOffIndex = prefs.getUChar("scroff", SCREEN_TIMEOUT_DEFAULT) % ui::kScreenOffChoices;
  language = static_cast<Lang>(prefs.getUChar("lang", static_cast<uint8_t>(DEFAULT_LANGUAGE)));
  net::setPortalCroatian(language == Lang::Hr);

  // Touch reset goes through the I/O expander; do it before the panel init.
  TouchInput::resetViaExpander();
  logI2cDevices();
  if (!touch.begin()) Serial.println("[Touch] no touch controller answered yet");

  const size_t bytes = SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX * sizeof(uint16_t);
  frame = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
  shown = static_cast<uint16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM));
  bandBuf = static_cast<uint16_t *>(
      heap_caps_malloc(SCREEN_WIDTH_PX * kFlushBand * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  Serial.printf("[Display] framebuffers %s, PSRAM free %u\n",
                frame && shown && bandBuf ? "ok" : "FAILED",
                ESP.getFreePsram());

  if (!panel->begin()) {
    Serial.println("[Display] panel init failed");
  }
  // begin() turns the panel on bright; hide it until the first frame is in.
  panel->setBrightness(0);
  panel->fillScreen(0x0000);

  net::begin();
  // Above the Arduino loop's priority so drawing never delays touch samples.
  i2cMutex = xSemaphoreCreateMutex();
  gestureQueue = xQueueCreate(8, sizeof(Gesture));
  xTaskCreatePinnedToCore(touchTask, "touch", 4096, nullptr, 2, nullptr, 1);
  audio::begin(i2cMutex);
  audio::setVolume(volumeSetting);
}

void loop() {
  static unsigned long lastFrameMs = 0;
  static Page shownPage = Page::Home;
  static bool shownAny = false;
  static UiState st;

  pollButtons();
  handleTouch(st);
  saveBrightnessIfIdle();
  pollBattery();
  sleepIfIdle();
  wakeOnLift();
  lightSleepIfIdle();

  // Time zone from the network task (saved one at boot, detected later).
  char tz[40];
  if (net::takeTimezone(tz, sizeof(tz))) {
    setenv("TZ", tz, 1);
    tzset();
  }
  chimeIfHour();

  const unsigned long now = millis();
  // Faster frames while the settings list is being scrolled.
  const bool scrolling = page == Page::Settings && settingsScroll != shownScroll;
  if (now - lastFrameMs < (scrolling ? 25UL : 1000UL / ANIMATION_FPS)) {
    delay(screenOn ? 2 : 20);
    return;
  }
  lastFrameMs = now;
  const int pi = static_cast<int>(page);
  // Each page's background is allocated the first time the page is shown.
  if (!bases[pi]) {
    bases[pi] = static_cast<uint16_t *>(heap_caps_malloc(
        SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  }
  uint16_t *base = bases[pi];
  if (!screenOn || !base || !frame || !shown || !bandBuf) return;

  st = UiState();
  buildState(st, now);
  st.scroll = settingsScroll;
  followSetup(st);

  const unsigned long drawStart = millis();
  const int W = SCREEN_WIDTH_PX, H = SCREEN_HEIGHT_PX;
  Canvas baseCanvas(base, W, H);
  Canvas frameCanvas(frame, W, H);
  static uint32_t frameNo = 0;
  static uint32_t lastDynKey = 0;
  frameNo++;

  // Per-stage timing (ms) for the slowest frame, printed in the heartbeat.
  unsigned long tStatic = 0, tCopy = 0, tDynamic = 0, tFlush = 0;
  unsigned long t = millis();

  // Static layer of this page: only when its content changed.
  const uint32_t key = ui::staticKey(st);
  bool fullFrame = !shownAny || shownPage != page || forceFullFrame;
  forceFullFrame = false;
  if (!baseValid[pi] || baseKeys[pi] != key) {
    ui::drawStatic(baseCanvas, st);
    baseKeys[pi] = key;
    baseValid[pi] = true;
    fullFrame = true;
  }
  // Settings rows: their own tall layer, redrawn when the content changes.
  if (page == Page::Settings) {
    if (!settingsList) {
      settingsList = static_cast<uint16_t *>(heap_caps_malloc(
          W * ui::kSettingsContentH * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    }
    if (settingsList && (!settingsListValid || settingsListKey != key)) {
      Canvas listCanvas(settingsList, W, ui::kSettingsContentH);
      ui::drawSettingsList(listCanvas, st);
      settingsListKey = key;
      settingsListValid = true;
      fullFrame = true;
    }
  }
  tStatic = millis() - t;
  t = millis();
  if (fullFrame) memcpy(frame, base, W * H * sizeof(uint16_t));
  tCopy = millis() - t;
  shownPage = page;
  shownAny = true;

  // Dynamic layer: skipped entirely when nothing in it would change.
  const uint32_t dynKey = ui::dynamicKey(st, frameNo);
  const bool dynamicChanged = fullFrame || dynKey != lastDynKey;
  lastDynKey = dynKey;
  Rect rects[4];
  int n = 0;
  int yFrom = H, yTo = 0;
  t = millis();
  if (dynamicChanged) {
    n = ui::dynamicRects(frameCanvas, st, rects);
    for (int i = 0; i < n; i++) {
      if (!fullFrame) restoreRect(base, rects[i]);
      yFrom = min(yFrom, rects[i].y);
      yTo = max(yTo, rects[i].y + rects[i].h);
    }
    ui::drawDynamic(frameCanvas, st);
  }
  tDynamic = millis() - t;

  // Visible window of the settings list.
  bool listMoved = false;
  if (page == Page::Settings && settingsList && (fullFrame || settingsScroll != shownScroll)) {
    if (!fullFrame) memcpy(frame, base, W * ui::kListTop * sizeof(uint16_t));
    ui::composeSettings(frameCanvas, settingsList, settingsScroll);
    shownScroll = settingsScroll;
    listMoved = true;
  }

  t = millis();
  if (fullFrame) {
    // Whole screen: send it as is, no point comparing every pixel first.
    panel->draw16bitRGBBitmap(0, 0, frame, W, H);
    memcpy(shown, frame, W * H * sizeof(uint16_t));
    panelHasFrame = true;
    flushedBands += H / kFlushBand;
  } else if (listMoved) {
    flushFrame(ui::kListTop, H);
  } else if (n > 0) {
    flushFrame(yFrom, yTo);
  }
  tFlush = millis() - t;

  static unsigned long worstStage[4] = {};
  const unsigned long total = millis() - drawStart;
  static unsigned long worstTotal = 0;
  if (total >= worstTotal) {
    worstTotal = total;
    worstStage[0] = tStatic; worstStage[1] = tCopy;
    worstStage[2] = tDynamic; worstStage[3] = tFlush;
  }

  // Heartbeat: proves the screen keeps refreshing and how long a frame takes.
  static unsigned long lastBeatMs = 0;
  static int frames = 0;
  static unsigned long worstMs = 0;
  frames++;
  worstMs = max(worstMs, millis() - drawStart);
  if (now - lastBeatMs > 30000) {
    Serial.printf("[UI] %02d:%02d wifi=%d weather=%d touch=%s frames=%d bands=%d "
                  "frame=%lums worst=%lums (static %lu, copy %lu, dynamic %lu, flush %lu)\n",
                  st.screen.hour, st.screen.minute, st.screen.wifiConnected ? 1 : 0,
                  st.screen.weather.valid ? 1 : 0, touch.chipName(), frames, flushedBands,
                  total, worstMs, worstStage[0], worstStage[1], worstStage[2],
                  worstStage[3]);
    lastBeatMs = now;
    frames = 0;
    flushedBands = 0;
    worstMs = 0;
    worstTotal = 0;
  }

  // Night never goes brighter than the user's own setting.
  const bool night = nightDim && st.screen.timeValid &&
                     (st.screen.hour >= NIGHT_START_HOUR || st.screen.hour < NIGHT_END_HOUR);
  const int dayLevel = (brightnessSetting * 255 + 50) / 100;
  setBrightness(night ? min<int>(dayLevel, BRIGHTNESS_NIGHT) : dayLevel);
}
