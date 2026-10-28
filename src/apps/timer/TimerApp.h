#pragma once

// Kitchen timer logic: choose minutes, count down, ring (old wind-up bell).
// Keeps counting on other pages and with the screen off. Drawing is in
// TimerUi.h.

#include "apps/timer/TimerUi.h"

namespace timer_app {

// Loads the last chosen time.
void begin();
// A button on the timer page.
void action(ui::TimerAction a, int preset);
// Every loop pass: notices the end, rings, stops ringing.
void update();
bool ringing();
// Running, paused or ringing.
bool active();
// State for the screen.
void fill(TimerInfo &out);

} // namespace timer_app
