#pragma once

// Speaker: ES8311 codec over I2S plus the amplifier enable pin. Sounds are
// synthesized (no audio files) and played by a small task, so play() returns
// at once. The codec shares the I2C bus with the PMU and touch chip; pass the
// same mutex they use.

#include <Arduino.h>

namespace audio {

void begin(SemaphoreHandle_t i2cMutex);

// 0..100 (0 = silent). Applied before the next sound.
void setVolume(int percent);

// Two-note "ding-dong" (hourly chime).
void playChime();

// One short soft note (volume preview).
void playBlip();

// Kitchen timer alarm: an old wind-up bell ringing for 2 s, at least half
// volume.
void playAlarm();

// Cuts the sound that is playing short (the alarm when the timer is stopped).
void stop();

// True from play*() until the sound has finished.
bool busy();

} // namespace audio
