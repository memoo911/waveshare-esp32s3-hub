#pragma once

// What the main program (main.cpp) offers the apps: settings storage, the
// screen, the page that is open, and the shared I2C bus. Apps use these
// instead of reaching into main.cpp directly.

#include <Preferences.h>

#include "hw/Imu.h"
#include "ui/UiState.h"

namespace sys {

// Settings saved on the board (NVS).
Preferences &prefs();
// UI language.
Lang language();
// The page on screen, and opening another one.
Page page();
void openPage(Page p);
// Turn the screen on (if it's off).
void wakeScreen();
// Counts as user activity: the screen-off countdown starts again.
void keepAwake();
// Motion sensor (spirit level).
Imu &imu();

// The touch chip, PMU, audio codec and motion sensor share one I2C bus that
// several tasks use: hold one of these while talking to it.
struct I2cLock {
  I2cLock();
  ~I2cLock();
};

} // namespace sys
