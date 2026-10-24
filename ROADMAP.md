# Roadmap

Target board: Waveshare ESP32-S3-Touch-AMOLED-1.8 V2 (CO5300, 368x448),
3.7 V 200 mAh Li-Po.

## 0. Verify the current build on hardware ✅

Clock and weather: panel, WiFi, NTP, Open-Meteo, animations,
battery, BOOT (refresh) and PWR (screen on/off).

## 1. Launcher ✅

Home screen with app tiles: **Weather** and **Settings** (more apps: see
step 7).

## 2. Weather app ✅

Open-Meteo covers the whole world, no API key. Requests use the selected
place's time zone (`timezone=auto`) so sunrise/sunset are local to that place.

- Current conditions with animated icons; at night the moon shows its phase
- Swipe right-to-left: 3-day forecast
- Refresh every 15 minutes; BOOT refreshes right away

## 3. Clock and time zone ✅

The clock always shows the time where the user is, independent of the
weather location. The time zone comes from, in order:

1. Automatic lookup by the network's public IP on each refresh (follows you
   when travelling on a phone hotspot)
2. The phone's time zone, sent by the WiFi/location setup page
3. Until one is known: the default in `Config.h` (Central European Time)

A manual choice in Settings is listed under Later. DST is handled
automatically from the zone's rules; NTP keeps the time exact.

## 4. Settings ✅ WiFi, location, display, sound, apps, language, turn off

1. **WiFi**: when offline, a QR code joins the board's temporary access point
   (random password); once a phone joins, a second QR code opens the form at
   192.168.4.1 (captive portals don't pop up reliably on Android). When
   online, the form is served on the home network instead, so a phone can add
   its own hotspot. Up to 5 networks, deletable on the device;
   `credentials.h` is optional and imported once.
2. **Location**: QR code with the board's local URL (e.g.
   `http://192.168.1.45`); the page searches places via the Open-Meteo
   geocoding API, lists matches, and saves the choice to NVS. Weather
   refreshes right away and the city name updates.
3. **Display**: brightness slider, screen-off time (1/2/5 min, never), night
   dimming switch (22:00-07:00, on by default).
4. **Sound**: see step 6.
5. **Apps**: placeholder for step 7.
6. **Language**: English (default) or Croatian, also for the phone pages.
7. **Turn off**: a confirmation page with a big red button switches the
   board off through the PMU, also on USB (verified); PWR turns it on.
The settings list scrolls with a finger, so more rows can be added below.
   Option rows are big (60 px) with tap areas that reach the next row.

## 5. Power saving and battery care ✅

- Screen off after 1, 2 (default) or 5 minutes without a touch or button
  press, or never (Settings -> Display); not while the WiFi setup or location
  page is open. A touch or the BOOT button wakes it; the waking touch doesn't
  open anything. PWR still turns it off/on.
- Wake when the board is picked up (QMI8658 accelerometer: tilt away from the
  resting position or a clear jolt), only after the screen went dark by
  itself. Works on hardware; watch for false wakes and tune
  (`LIFT_TILT_DEG`, `LIFT_JOLT_G`) if needed.
- Battery care: on USB, charging pauses at 100 % and resumes only when the
  battery has dropped to 90 % (`BATTERY_CARE` in `Config.h`). Resuming at 95 % made it cycle every half hour: right after charging stops the voltage relaxes and the gauge shows 95-96 %.

## 6. Sound ✅ (hourly chime still to hear on hardware)

The speaker works (ES8311 codec). No sounds on touch.

- **Volume** slider in Settings → Sound, like brightness; a short note plays
  when the finger lets go, so you hear the level
- **Hourly chime**: two-note "ding-dong" on the full hour, on/off in
  Settings → Sound, **off** by default, silent at night (22:00-07:00)

## Next (planned order)

### 7. Apps ✅ app switcher, kitchen timer, spirit level (verified on hardware)

Settings → **Apps** chooses the app on the home screen next to Settings:
Weather (default), **Kitchen timer** or **Spirit level**.

- **Kitchen timer**: − / + and 3/5/8/10 min presets (last time remembered),
  countdown ring, pause/resume/reset; at 0:00 the screen wakes and it beeps
  once a second for 10 seconds (at least half volume, also at night), tap or
  BOOT stops it. Keeps counting on other pages and with the screen off; the
  screen stays on while the countdown is shown.
- **Spirit level**: round bubble level when flat (X/Y and total tilt), tube
  level when standing on the bottom or side edge, green "Level" within 0.5°,
  "Set zero" per mode (saved), ~40 fps while open, screen stays on. Axis
  mapping for the V2 board confirmed on hardware (`LEVEL_SWAP_XY` and
  `LEVEL_FLIP_X` on). Each board reads a couple of degrees off flat, which
  "Set zero" removes. Possible later: two-step zero (tap, turn 180°, tap) that
  also works on a surface that isn't level.

Later: alarm clock, calculator, music player (SD card).

### 8. Updates over WiFi (OTA)

Upload a new version without the USB cable: from VS Code over the home
network (PlatformIO `upload_protocol = espota`), with a password so nobody
else on the network can flash the board.

### 9. Weather extras

- Today by the hour (swipe up on the weather screen): temperature and rain
  chance for the next 12 hours
- Optional rain warning: a short sound / notice when rain is expected within
  the next hour

### 10. Battery life ✅ (light sleep on battery; fine-tuning optional)

Done:
- `[Power]` log line every 10 minutes (level, voltage, USB, screen, CPU, WiFi)
- Screen off: WiFi only for the periodic refresh, off in between; CPU at
  80 MHz; slower touch and loop polling
- Switch off at 3 % on battery power

Measured (first round, screen off, on battery): about 97 % → 84 % in 38
minutes, roughly 5 hours from full. Mostly the chip staying awake at 80 MHz.

Second round (measured: 90 % → 82 % in 70 minutes with a few screen-ons,
about 7 % per hour, roughly 13-14 hours from full):
- Light sleep on battery with the screen off and WiFi resting: 300 ms naps,
  woken early by the touch interrupt or BOOT; the timer wake-ups keep the
  clock, chime, refresh schedule, PWR button and pick-up check going. Not on
  USB, so the serial monitor stays connected.

Optional fine-tuning:
- Longer naps (500 ms) and shorter awake windows, about 4-5 % per hour;
  pick-up reacts up to half a second later
- Fewer animation frames when nothing moves
- Check which on-board parts (codec, touch, display rails) can be powered down
- Power info page: battery voltage, charging state, estimated time left
- Turn the board off safely below ~3 % instead of letting the battery run flat

## Later

- Manual time zone as a fallback (the automatic one follows the network and
  is independent of the forecast place: weather for Hong Kong keeps the clock
  on local time)
- Weather card: local time of the forecast place when it is in another time
  zone (e.g. "New York · 09:05")
- Network details, diagnostics, about page

## Decisions

- UI language defaults to English for anyone cloning the repo; Croatian is a
  setting. All strings go through one translation table.
- The clock follows the user's time zone; the forecast follows the chosen
  place. The two are independent.
- Keep the in-house renderer (anti-aliased shapes, Inter font with Croatian
  letters) instead of LVGL.
- Text entry (passwords, places) happens on the phone via QR + web page, not
  on the 1.8" on-screen keyboard.
- Code under the MIT license; third-party parts keep theirs
  (`licenses/THIRD_PARTY.md`).
- No personal data in the repo: WiFi passwords stay on the board (or in the
  git-ignored `credentials.h`), screenshots use example names.
- Big rows and generous tap areas: the screen is small.
