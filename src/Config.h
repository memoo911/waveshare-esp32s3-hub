#pragma once

#include <Arduino.h>

#include "I18n.h"

// ============= Defaults (changed on the device in Settings) =============
// Place for the forecast until one is picked from a phone.
constexpr const char *DEFAULT_PLACE_NAME = "Zagreb";
constexpr float DEFAULT_PLACE_LAT = 45.81f;
constexpr float DEFAULT_PLACE_LON = 15.98f;
// UI language until changed in Settings.
constexpr Lang DEFAULT_LANGUAGE = Lang::En;

// ============= Time =============
// Clock time zone until it is detected from the network (by IP address).
// Central Europe with automatic summer/winter time.
constexpr const char *DEFAULT_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";
// How often to re-check the time zone (also catches summer/winter changes).
constexpr unsigned long TZ_LOOKUP_MS = 60UL * 60UL * 1000UL;
constexpr const char *NTP_SERVER_1 = "pool.ntp.org";
constexpr const char *NTP_SERVER_2 = "time.google.com";

// ============= Weather =============
// Open-Meteo updates every 15 minutes; no API key needed.
constexpr unsigned long WEATHER_REFRESH_MS = 15UL * 60UL * 1000UL;
// Retry sooner after a failed fetch.
constexpr unsigned long WEATHER_RETRY_MS = 60UL * 1000UL;
// Mark data as old ("· staro") when the last good fetch is older than this.
constexpr unsigned long WEATHER_STALE_MS = 60UL * 60UL * 1000UL;

// ============= Display =============
constexpr int SCREEN_WIDTH_PX = 368;
constexpr int SCREEN_HEIGHT_PX = 448;
// Animation frames per second for the weather icon (rain, snow, stars...).
constexpr int ANIMATION_FPS = 10;
// Default brightness in percent; changed in Settings and saved on the board.
constexpr int BRIGHTNESS_DEFAULT_PERCENT = 80;
// Upper limit 0-255 at night (NIGHT_START_HOUR..NIGHT_END_HOUR).
constexpr uint8_t BRIGHTNESS_NIGHT = 50;
constexpr int NIGHT_START_HOUR = 22;
constexpr int NIGHT_END_HOUR = 7;
// Screen turns off after a while without a touch or button press (Settings ->
// Display: 1, 2 or 5 minutes, or never; 2 by default). A touch, the BOOT
// button or picking the board up wakes it. Not while the WiFi setup or
// location page is open (the phone is busy with the QR code).
constexpr int SCREEN_TIMEOUT_DEFAULT = 1; // index into the choices: 1, 2, 5 min, never
// Picking up: tilt away from the resting position, or two quick jolts in a
// row (QMI8658 accelerometer, read every LIFT_POLL_MS while the screen is off).
constexpr float LIFT_TILT_DEG = 15.0f;
constexpr float LIFT_JOLT_G = 0.12f;
constexpr unsigned long LIFT_POLL_MS = 40;

// ============= Sound =============
// Default volume in percent; changed in Settings and saved on the board.
constexpr int VOLUME_DEFAULT_PERCENT = 60;
// The hourly chime (off by default) stays silent at night
// (NIGHT_START_HOUR..NIGHT_END_HOUR).

// Spirit level: how the accelerometer axes map to the screen (x to the right,
// y down). If the bubble moves the wrong way, flip or swap here; the serial
// log prints the raw values once a second while the level is open.
// On the V2 board the sensor's X axis runs along the screen's height and its
// Y axis along the width, reversed (both checked on hardware), hence the
// swap and the flip.
constexpr bool LEVEL_SWAP_XY = true;
constexpr bool LEVEL_FLIP_X = true;
constexpr bool LEVEL_FLIP_Y = false;

// Assistant app: hold BOOT and speak, at most ASSISTANT_MAX_RECORD_MS; a
// shorter press than ASSISTANT_MIN_RECORD_MS is ignored. The Worker address
// and device key go in credentials.h (see credentials.h.example).
constexpr unsigned long ASSISTANT_MAX_RECORD_MS = 15UL * 1000UL;
constexpr unsigned long ASSISTANT_MIN_RECORD_MS = 600;
// Questions remembered in one conversation (older ones are dropped).
constexpr int ASSISTANT_MAX_TURNS = 8;

// Kitchen timer: when time is up, an old wind-up bell rings for 2 s every
// TIMER_RING_EVERY_MS, for TIMER_RING_MS in total (4 rings).
constexpr unsigned long TIMER_RING_MS = 10UL * 1000UL;
constexpr unsigned long TIMER_RING_EVERY_MS = 2500;

// ============= Battery care =============
// On USB, charging pauses at BATTERY_FULL_PERCENT and resumes only once the
// battery has dropped to BATTERY_RESUME_PERCENT, so a clock that lives on the
// cable doesn't sit at 100 % being topped up all the time.
constexpr bool BATTERY_CARE = true;
constexpr int BATTERY_FULL_PERCENT = 100;
// 90 rather than 95: right after charging stops the voltage relaxes and the
// gauge drops to 95-96 % without any real use, which restarted charging
// every half hour.
constexpr int BATTERY_RESUME_PERCENT = 90;
// On battery (no USB), switch the board off at this level instead of letting
// the Li-Po run flat; PWR turns it on again once it's charged.
constexpr int BATTERY_SHUTDOWN_PERCENT = 3;

// ============= Power saving (screen off) =============
// CPU clock while the screen is off (240 MHz while it's on). WiFi is only on
// for the periodic refresh, see Net.h setLowPower().
constexpr uint32_t CPU_MHZ_SCREEN_OFF = 80;
// On battery with the screen off and WiFi resting, the chip light-sleeps in
// steps of LIGHT_SLEEP_MS and wakes early on a touch or BOOT. Not on USB, so
// the serial monitor keeps working while you're developing.
constexpr bool LIGHT_SLEEP = true;
constexpr unsigned long LIGHT_SLEEP_MS = 300;
// Battery level, voltage and power state in the serial log, to measure how
// long the battery lasts.
constexpr unsigned long POWER_LOG_MS = 10UL * 60UL * 1000UL;

// ============= Hardware (Waveshare ESP32-S3-Touch-AMOLED-1.8) =============
// Same pins as the chat-stick waveshare firmware.
constexpr int LCD_SDIO0_PIN = 4;
constexpr int LCD_SDIO1_PIN = 5;
constexpr int LCD_SDIO2_PIN = 6;
constexpr int LCD_SDIO3_PIN = 7;
constexpr int LCD_SCLK_PIN = 11;
constexpr int LCD_CS_PIN = 12;
#if defined(WAVESHARE_PANEL_CO5300)
// V2 panel: 368 visible columns start at controller column 16.
constexpr uint8_t LCD_PANEL_COL_OFFSET = 16;
#else
constexpr uint8_t LCD_PANEL_COL_OFFSET = 0;
#endif

constexpr int BOARD_I2C_SDA_PIN = 15;
constexpr int BOARD_I2C_SCL_PIN = 14;
// BOOT button, active low: press to refresh the weather now.
constexpr int BUTTON_BOOT_PIN = 0;
// Speaker: ES8311 codec (I2C 0x18) on I2S, amplifier enabled by GPIO46.
constexpr int AUDIO_I2S_MCLK_PIN = 16;
constexpr int AUDIO_I2S_BCLK_PIN = 9;
constexpr int AUDIO_I2S_WS_PIN = 45;
constexpr int AUDIO_I2S_DOUT_PIN = 8;
// Microphone (analog, through the ES8311's ADC) comes back on this pin.
constexpr int AUDIO_I2S_DIN_PIN = 10;
constexpr int AUDIO_PA_ENABLE_PIN = 46;

// ============= Touch =============
// Touch interrupt (FT3168 / CST820). Coordinates map 1:1 to the screen; if a
// tap lands in the wrong place, flip or swap axes here.
constexpr int TOUCH_INT_PIN = 21;
constexpr bool TOUCH_SWAP_XY = false;
constexpr bool TOUCH_FLIP_X = false;
constexpr bool TOUCH_FLIP_Y = false;
// Touch sampling interval; 10 ms = 100 samples per second.
constexpr unsigned long TOUCH_POLL_MS = 10;

// ============= WiFi =============
struct WiFiNetwork {
  const char *ssid;
  const char *password;
};
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10000;
// Wait between connection rounds when no network answers.
constexpr unsigned long WIFI_RETRY_MS = 30000;
// WiFi setup from a phone: the board's own network is "<prefix>-XXXX".
constexpr const char *SETUP_AP_PREFIX = "Clock";
// Give up on setup mode after this long.
constexpr unsigned long SETUP_TIMEOUT_MS = 10UL * 60UL * 1000UL;
