# Waveshare AMOLED weather clock

**English** · [Hrvatski](README.hr.md)

A clock and weather station for the **Waveshare ESP32-S3-Touch-AMOLED-1.8**:
big clock, current weather with animated icons, a 3-day forecast, and
settings you change from your phone by scanning a QR code. No API keys, no
app to install.

![Home, weather, forecast and night screens](docs/screens.png)

## Features

- **Clock** with date, synced over the internet; time zone and summer/winter
  time are detected automatically
- **Current weather** for any place in the world: temperature, feels-like,
  low/high, humidity, wind, sunrise/sunset ([Open-Meteo](https://open-meteo.com))
- **Animated icons**: rain falls, snow drifts, the sun turns, lightning
  flashes; at night the moon shows its real phase
- **3-day forecast**: swipe left on the weather screen
- **Touch screen**: home screen with app tiles, swipe right to go back
- **Settings**: WiFi and location from your phone via QR code, brightness
  and volume sliders, screen-off time, night dimming, hourly chime, language,
  turn off
- **Sound**: optional hourly chime (off by default, silent at night)
- **Languages**: English (default) and Croatian (Hrvatski) for now, on the
  board and on the phone pages
- **Battery** level and charging indicator; dimmer screen at night (can be
  switched off)
- **Screen sleep**: off after 1, 2 or 5 minutes (or never); a touch or picking
  the board up wakes it
- **Battery care**: on USB, charging stops at 100 % and starts again at 95 %

## What you need

- **Waveshare ESP32-S3-Touch-AMOLED-1.8**. Check the sticker on the back:
  boards marked **V2** (CO5300 display) are the default here. Older boards
  without V2 have an SH8601 display (see [Board versions](#board-versions)).
- A **USB-C cable that carries data** (some charging-only cables don't).
- Optional: a **3.7 V Li-Po battery with an MX1.25 2-pin plug** (Waveshare
  sells a matching one). Check that the red wire goes to `+` on the `BAT`
  connector before plugging it in.
- A computer (Windows, macOS or Linux) and a phone.

## 1. Install the tools (once)

1. **VS Code**: download and install from
   [code.visualstudio.com](https://code.visualstudio.com).
2. **PlatformIO extension** (it builds and uploads the program):
   1. Open VS Code.
   2. Click the **Extensions** icon in the left bar (four squares), or press
      `Ctrl+Shift+X` (`Cmd+Shift+X` on a Mac).
   3. Search for **PlatformIO IDE** and click **Install**.
   4. Wait until it finishes (it downloads its tools, a few minutes the first
      time), then restart VS Code if it asks. An **ant icon** appears in the
      left bar when it's ready.
3. **Git** (to download the code): install from
   [git-scm.com](https://git-scm.com). You can skip this and use
   **Code → Download ZIP** on the GitHub page instead.

## 2. Get the code

In VS Code open a terminal (**Terminal → New Terminal**), go to the folder
where you keep projects and run:

```
git clone https://github.com/memoo911/waveshare-weather.git
cd waveshare-weather
git config core.hooksPath .githooks
```

The last line turns on a safety check that stops you from ever committing a
file with your WiFi password by accident.

Then **File → Open Folder…**, pick the `waveshare-weather` folder and click
**Select Folder**. Wait a few seconds until PlatformIO has loaded: a blue bar
at the bottom shows a **✓** (Build) and **→** (Upload) button.

## 3. Upload the program to the board

1. Connect the board to the computer with the USB-C cable.
2. **Turn the board on**: press the **PWR** button once. It only shows up on
   the computer while it's on (as a COM port on Windows).
3. In the blue bar at the bottom, check that it says **`env:waveshare-v2`**
   (click it to change it).
4. Click **→ Upload** in the blue bar, or press `Ctrl+Alt+U`.
5. The first build takes a few minutes (libraries are downloaded). It ends
   with **SUCCESS** and the board restarts with the new program.

**If the upload can't connect** (`Failed to connect`, no serial port):
hold **BOOT**, press **PWR** once, release **BOOT**, then upload again.
Afterwards press **PWR** (or unplug and replug) to start normally.

**To see what the board is doing**, click the **plug icon** in the blue bar
(Serial Monitor) or press `Ctrl+Alt+S`.

## Updating to a new version

When there is new code on GitHub:

1. In VS Code open a terminal (**Terminal → New Terminal**) in the
   `waveshare-weather` folder and run:
   ```
   git pull
   ```
2. Turn the board on (**PWR**) and connect it with the USB-C cable.
3. Press `Ctrl+Alt+U` (Windows), or click **→ Upload** in the blue bar.
4. Wait for **SUCCESS**; the board restarts with the new version. Your saved
   networks, place, brightness, sound and language stay as they were.

## 4. First start

The screen shows the home screen. Tap **Settings** to set the board up.

### Settings at a glance

The list scrolls: drag it up to see the rows at the bottom.

| Row | What it does |
|---|---|
| **WiFi** | Saved networks, add or delete one (see below) |
| **Location** | Place for the forecast, chosen on your phone |
| **Display** | Brightness, when the screen turns off, night dimming |
| **Sound** | Volume, hourly chime |
| **Apps** | Reserved for later |
| **Language** | English / Croatian, tap to switch |
| **Turn off** | Switch the board off (press **PWR** to turn it on) |

Swipe right or tap **‹** at the top left to go back.

### Language

Two languages are available for now: **English** (default) and **Croatian
(Hrvatski)**. To switch, open **Settings** and tap the **Language** row; tap
it again to switch back. The choice is remembered and also applies to the
setup and location pages on your phone. More languages can be added in
`src/I18n.h`.

### Connect to WiFi

The board needs a **2.4 GHz** WiFi network (most routers have one; 5 GHz-only
networks don't work). For a phone hotspot: on an iPhone turn on **Maximize
Compatibility**, on Android choose the **2.4 GHz** band.

![WiFi setup steps](docs/wifi-setup.png)

1. On the board open **Settings → WiFi → Add network**.
2. A QR code appears (left picture). **Scan it with the phone camera** and
   accept joining the network **Clock-XXXX**. If the phone warns that the
   network has no internet, choose to **stay connected**.
3. As soon as the phone is connected, the board shows a **second QR code**
   (middle picture). **Scan it** to open the setup page. (Some phones open the
   page by themselves; you can also type `192.168.4.1` in the browser.)
4. Type the **network name** (or tap one of the nearby networks) and the
   **password**, then tap **Save**.
5. The board closes its own network and connects. Your phone goes back to its
   usual WiFi.

**Adding more networks later** (for example your phone's hotspot): when the
board is already online, **Add network** shows a QR code with the board's
address instead (right picture). Your phone stays on the same WiFi, so you
can add a network that is off right now, like the hotspot of the phone you're
holding. The board connects to whichever saved network is in range.

**Deleting a network**: on the **WiFi** page tap **×** next to it, then tap
**×** again to confirm. A green dot marks the network in use.

### Choose the place for the forecast

![Settings, WiFi, location, display and sound](docs/settings.png)

1. On the board open **Settings → Location**. A QR code appears.
2. Make sure your phone is on the **same WiFi** as the board, then scan it.
3. On the page that opens, type a city or village (for example *Zagreb*) and
   tap **Search**.
4. Tap the right result (the region and country are shown under each name).
5. The board saves the place and shows its weather a few seconds later.

The clock keeps showing **your** local time, wherever the forecast is for.

### Sound

Open **Settings → Sound**.

- **Volume**: drag the slider (0 % is silent). When you let go, a short note
  plays so you hear the new level. The board makes no sounds when you tap.
- **Hourly chime**: tap anywhere below the slider (on the **Hourly chime**
  row) to switch it on or off. The switch turns green when it's on. It plays a
  short "ding-dong" on every full hour, also while the screen is off, but
  stays silent from 22:00 to 07:00. It is off until you turn it on.

### Screen and battery

- **Night dimming**: from 22:00 to 07:00 the screen is at most about 20 %
  bright, whatever the brightness slider says, so it doesn't light up a dark
  bedroom. It's on by default; switch it off in **Settings → Display →
  Night dimming**.
- The screen turns off after **2 minutes** without a touch. To change that,
  open **Settings → Display** and tap the **Screen off after** row: it steps
  through **1 min**, **2 min**, **5 min** and **Never**.
- To wake it, **touch** the screen, press **BOOT**, or **pick the board up**.
  The touch that wakes it doesn't open anything. Picking up only works when
  the screen went dark by itself: after a **PWR** press it stays off until you
  touch it or press a button.
- While the WiFi setup or location QR code is showing, the screen stays on.
- **Battery care**: while the board is on USB, charging stops at 100 % and
  starts again only when the battery has dropped to 95 %. Meanwhile the board
  runs from USB, so the lightning bolt next to the battery may be missing even
  though the cable is plugged in. This keeps a clock that lives on the cable
  from wearing out its battery.

### Turning off

Scroll to the bottom of **Settings** and tap **Turn off**. A page with a big
red **Turn off** button appears; tap it and the board switches off completely,
also when it's on USB. **Cancel** (or swiping right) goes back. Press **PWR**
once to turn it on again; all settings are kept. Holding **PWR** for 6
seconds also turns it off.

### Apps

**Settings → Apps** is reserved for later: the idea is to choose one
app for the home screen (for example a calculator or an AI assistant) instead
of the weather. It does nothing yet.

## Everyday use

| Action | What it does |
|---|---|
| Tap a tile on the home screen | Opens Weather or Settings |
| Swipe **left** on the weather screen | 3-day forecast |
| Swipe **right**, or tap **‹** at the top left | Back |
| **Settings → Display** | Drag the brightness slider; tap **Screen off after** to choose when the screen turns off; tap **Night dimming** to switch the dimmer night screen (22:00 to 07:00) on or off |
| **Settings → Sound** | Drag the volume slider (a short note plays when you let go); tap below it to switch the hourly chime on or off |
| **BOOT** button | Refresh the weather now ("Refreshing…" shows at the top); wakes the screen when it's off |
| Touch the dark screen, or pick the board up | Wakes it |
| **PWR**, short press | Screen off / on |
| Drag up / down in **Settings** | Scroll the list (more rows below: Apps, Language, Turn off) |
| **Settings → Turn off** | Opens a confirmation: tap the red **Turn off** button and the board switches off completely, also on USB (**Cancel** goes back). Press **PWR** to turn it on |
| **PWR**, hold 6 seconds | Turn the board off (press once to turn it on) |

The weather refreshes every 15 minutes. Settings, saved networks and the
place are kept when the board is turned off.

## Troubleshooting

| Problem | Try this |
|---|---|
| `UnknownBoard` or build errors after updating | Pull the latest code; the project pins the library versions it needs |
| Board doesn't appear on the computer | Turn it on with **PWR**; try another USB-C cable (must carry data) |
| "Can't find *network*" | The network is out of range or 5 GHz only; use a 2.4 GHz network |
| "Wrong password for *network*?" | Delete it on the WiFi page and add it again |
| No sound | Check **Settings → Sound**: volume above 0 %. The chime is off by default and silent from 22:00 to 07:00 |
| On USB but no lightning bolt | Normal with battery care: charging restarts at 95 % |
| Screen wakes up by itself, or not when picked up | Adjust `LIFT_TILT_DEG` / `LIFT_JOLT_G` in `src/Config.h` (higher = less sensitive), or set **Screen off after → Never** |
| Screen never turns off | **Settings → Display → Screen off after** is set to **Never**, or the WiFi setup / location QR page is open |
| Setup page doesn't open on the phone | Scan the second QR code, or open `192.168.4.1` in the browser. On Samsung phones, turning off **Settings → Connections → More connection settings → Private DNS** for a moment helps |
| Location page doesn't load | Phone and board must be on the same WiFi |

## Board versions

Two panels exist. Pick the matching environment in the blue bar:

- **`waveshare-v2`**: V2 boards (sticker says V2, CO5300 display). Default.
- **`waveshare-v1`**: original boards (SH8601 display). This needs a newer
  Arduino core than the standard PlatformIO platform provides and is untested.

## Configuration

Defaults live in `src/Config.h`: the starting place, language and time zone
(used until you change them on the board), refresh intervals, night hours,
brightness limits, default volume and screen-off time, how sensitive waking on
pick-up is (`LIFT_TILT_DEG`, `LIFT_JOLT_G`) and battery care
(`BATTERY_CARE`, `BATTERY_FULL_PERCENT`, `BATTERY_RESUME_PERCENT`).

`src/credentials.h` is **optional**. If you prefer to list WiFi networks in
code, copy `src/credentials.h.example` to `src/credentials.h` and fill it in.
They are copied into the board's saved networks once; after that you manage
them on the device like any other. `credentials.h` is in `.gitignore`, and the
commit check blocks it as well.

## Privacy

- WiFi passwords are stored only on the board (and in `credentials.h` if you
  use it, which never leaves your computer).
- The board contacts: [Open-Meteo](https://open-meteo.com) with the chosen
  place's coordinates, [ip-api.com](https://ip-api.com) to find the time zone
  from your public IP address, and NTP time servers.
- The location page and the setup page only run while those pages are open
  on the board.

## Development

- `tools/preview.cpp` renders every screen on a PC with the same drawing code
  as the board, which is how the pictures above were made:
  ```
  gcc -O2 -c lib/qrcodegen/qrcodegen.c -o qrcodegen.o
  g++ -std=c++17 -O2 -Isrc -Ilib/qrcodegen tools/preview.cpp qrcodegen.o -o preview
  ./preview out/
  ```
- `tools/gen_fonts.py` regenerates `src/Fonts.h` from the Inter typeface.
- Code layout: `main.cpp` (display, touch, buttons, pages), `Net.cpp` (WiFi,
  weather, time zone, phone pages), `Ui.h` / `Screen.h` (page layouts),
  `Icons.h` (weather icons), `Render.h` (anti-aliased drawing), `Audio.cpp`
  (speaker and sounds), `I18n.h`
  (English and Croatian text). See `ROADMAP.md` for what's next.

## Credits

- Weather and place search: [Open-Meteo](https://open-meteo.com) (free for
  non-commercial use)
- Time zone lookup: [ip-api.com](https://ip-api.com) (free for
  non-commercial use)
- Font: [Inter](https://rsms.me/inter) by Rasmus Andersson, SIL Open Font
  License 1.1
- Audio codec driver: ES8311 driver by Espressif, Apache License 2.0
  (`lib/es8311`)
- QR codes: [QR Code generator library](https://www.nayuki.io/page/qr-code-generator-library)
  by Project Nayuki, MIT License (`lib/qrcodegen`)
- Display driver: [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX);
  power management: [XPowersLib](https://github.com/lewisxhe/XPowersLib);
  JSON: [ArduinoJson](https://arduinojson.org)

## License

The code is released under the [MIT License](LICENSE): use it, change it,
share it. Third-party parts keep their own licenses: the Inter font (SIL Open
Font License 1.1, [licenses/Inter-OFL.txt](licenses/Inter-OFL.txt)), the QR
code library (MIT) and the ES8311 driver (Apache 2.0); see
[licenses/THIRD_PARTY.md](licenses/THIRD_PARTY.md). Open-Meteo and
ip-api.com are free for non-commercial use; check their terms before using
the clock commercially.
