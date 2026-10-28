#pragma once

// Spirit level logic: reads the motion sensor, smooths it, decides flat or on
// an edge, applies the saved zero. Drawing is in LevelUi.h.

#include "ui/UiState.h"

namespace level_app {

// Loads the saved zero offsets.
void begin();
// The page was opened: start smoothing afresh.
void open();
// Current reading, calibrated.
void read(LevelInfo &out);
// "Set zero": the current position counts as level from now on.
void setZero();

} // namespace level_app
