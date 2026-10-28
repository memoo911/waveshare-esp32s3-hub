#pragma once

// Speaker and microphone: ES8311 codec over I2S plus the amplifier enable pin. Sounds are
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

// --- Microphone -------------------------------------------------------------
// Sample rate of the recording (the codec runs at 24 kHz, halved).
constexpr int kMicRate = 12000;

// Starts recording (at most maxMs). False if a sound is playing or the codec
// isn't ready. Stops by itself at maxMs.
bool startRecording(unsigned long maxMs);
void stopRecording();
// True from startRecording() until the samples are ready.
bool recording();
// Recent loudness 0..1, for the level meter.
float micLevel();
// The last recording: mono, 16-bit, kMicRate, DC removed and normalised.
// Valid until the next startRecording().
const int16_t *recordedSamples(size_t &count);

} // namespace audio
