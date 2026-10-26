#include "Audio.h"

#include <driver/i2s.h>
#include <math.h>

#include "Config.h"
#include "es8311.h"

namespace audio {
namespace {

constexpr i2s_port_t kPort = I2S_NUM_0;
constexpr int kRate = 24000;
constexpr int kChunk = 256; // frames per write

enum Sound : uint32_t { kNone = 0, kChime = 1, kBlip = 2, kAlarm = 3 };
// The timer alarm is never quieter than this, even with the volume at 0 %.
constexpr int kAlarmMinVolume = 50;

SemaphoreHandle_t i2c = nullptr;
TaskHandle_t task = nullptr;
volatile int volume = 60;
es8311_handle_t codec = nullptr;
bool ready = false;
volatile bool playing = false;
volatile bool abortSound = false;

struct I2cGuard {
  I2cGuard() { if (i2c) xSemaphoreTake(i2c, portMAX_DELAY); }
  ~I2cGuard() { if (i2c) xSemaphoreGive(i2c); }
};

bool initHardware() {
  i2s_config_t cfg = {};
  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = kRate;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = 0;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = kChunk;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("[Audio] I2S install failed");
    return false;
  }
  i2s_pin_config_t pins = {};
  pins.mck_io_num = AUDIO_I2S_MCLK_PIN;
  pins.bck_io_num = AUDIO_I2S_BCLK_PIN;
  pins.ws_io_num = AUDIO_I2S_WS_PIN;
  pins.data_out_num = AUDIO_I2S_DOUT_PIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(kPort, &pins);

  I2cGuard lock;
  codec = es8311_create(0, ES8311_ADDRESS_0);
  const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = kRate * 256,
      .sample_frequency = kRate,
  };
  if (!codec || es8311_init(codec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
    Serial.println("[Audio] ES8311 init failed");
    return false;
  }
  Serial.println("[Audio] ready");
  return true;
}

// Volume percent to the codec's DAC level: 1 % is about -45 dB, 100 % is
// +6 dB (the samples are written at half scale).
void applyVolume(int v) {
  int setting = 0;
  if (v > 0) {
    const float db = -45.0f + 51.0f * v / 100.0f;
    const int reg = 0xBF + static_cast<int>(lroundf(db * 2)); // 0.5 dB steps, 0xBF = 0 dB
    setting = (constrain(reg, 1, 255) + 1) * 100 / 256 + 1;
  }
  I2cGuard lock;
  es8311_voice_volume_set(codec, constrain(setting, 0, 100), nullptr);
}

// A bell-like note: fundamental plus a softer upper partial, quick attack and
// exponential decay.
struct Note {
  float freq;
  float start; // seconds
  float length;
  float decay; // time constant, seconds
};

void render(const Note *notes, int count, float total) {
  static int16_t buf[kChunk * 2];
  const int frames = static_cast<int>(total * kRate);
  for (int f0 = 0; f0 < frames; f0 += kChunk) {
    const int n = min(kChunk, frames - f0);
    for (int i = 0; i < n; i++) {
      const float t = static_cast<float>(f0 + i) / kRate;
      float s = 0;
      for (int k = 0; k < count; k++) {
        const Note &nt = notes[k];
        const float u = t - nt.start;
        if (u < 0 || u > nt.length) continue;
        const float attack = min(1.0f, u / 0.006f);
        const float release = min(1.0f, (nt.length - u) / 0.03f);
        const float env = attack * release * expf(-u / nt.decay);
        const float w = 2 * PI * nt.freq * u;
        s += env * (sinf(w) + 0.35f * sinf(2.76f * w) + 0.12f * sinf(5.4f * w));
      }
      const int16_t v = static_cast<int16_t>(constrain(s * 0.45f, -1.0f, 1.0f) * 32767);
      buf[i * 2] = v;
      buf[i * 2 + 1] = v;
    }
    size_t written = 0;
    i2s_write(kPort, buf, n * 4, &written, portMAX_DELAY);
  }
}

// Old wind-up kitchen timer bell: a small hammer strikes the bell about 22
// times a second for ~2 s. Each strike adds to a ringing envelope that decays
// between strikes; the bell has inharmonic partials like a real metal dome.
void renderBell(float total) {
  static int16_t buf[kChunk * 2];
  const float f1 = 2398.0f;
  const float strikeHz = 22.0f;
  const float ringStop = total - 0.25f; // hammer stops, bell rings out
  const float decayPerSample = expf(-1.0f / (0.05f * kRate));
  const int frames = static_cast<int>(total * kRate);
  float env = 0;
  int nextStrike = 0;
  int strikeCount = 0;
  for (int f0 = 0; f0 < frames && !abortSound; f0 += kChunk) {
    const int n = min(kChunk, frames - f0);
    for (int i = 0; i < n; i++) {
      const int f = f0 + i;
      const float t = static_cast<float>(f) / kRate;
      if (f >= nextStrike && t < ringStop) {
        // Strikes alternate slightly in strength, like a real clapper.
        env += (strikeCount++ & 1) ? 0.30f : 0.40f;
        if (env > 1.2f) env = 1.2f;
        nextStrike += static_cast<int>(kRate / strikeHz);
      }
      env *= decayPerSample;
      const float w = 2 * PI * f1 * t;
      const float s = env * (sinf(w) + 0.6f * sinf(2.321f * w) + 0.12f * sinf(3.2f * w) +
                             0.13f * sinf(4.257f * w));
      const float fade = min(1.0f, (total - t) / 0.03f);
      const int16_t v = static_cast<int16_t>(constrain(s * 0.4f * fade, -1.0f, 1.0f) * 32767);
      buf[i * 2] = v;
      buf[i * 2 + 1] = v;
    }
    size_t written = 0;
    i2s_write(kPort, buf, n * 4, &written, portMAX_DELAY);
  }
}

void play(uint32_t sound) {
  const int v = sound == kAlarm ? max(static_cast<int>(volume), kAlarmMinVolume) : volume;
  if (v <= 0) return;
  if (!ready) ready = initHardware();
  if (!ready) return;
  applyVolume(v);
  i2s_start(kPort);
  digitalWrite(AUDIO_PA_ENABLE_PIN, HIGH);
  delay(30); // amplifier settles
  if (sound == kChime) {
    const Note notes[] = {{1318.5f, 0.0f, 1.2f, 0.45f}, {1046.5f, 0.45f, 1.6f, 0.6f}};
    render(notes, 2, 2.1f);
  } else if (sound == kAlarm) {
    renderBell(2.0f);
  } else {
    const Note notes[] = {{1046.5f, 0.0f, 0.25f, 0.08f}};
    render(notes, 1, 0.3f);
  }
  i2s_zero_dma_buffer(kPort);
  delay(40);
  digitalWrite(AUDIO_PA_ENABLE_PIN, LOW);
  i2s_stop(kPort);
}

void audioTask(void *) {
  for (;;) {
    uint32_t sound = kNone;
    xTaskNotifyWait(0, UINT32_MAX, &sound, portMAX_DELAY);
    abortSound = false;
    if (sound != kNone) play(sound);
    playing = false;
  }
}

} // namespace

void begin(SemaphoreHandle_t i2cMutex) {
  i2c = i2cMutex;
  pinMode(AUDIO_PA_ENABLE_PIN, OUTPUT);
  digitalWrite(AUDIO_PA_ENABLE_PIN, LOW);
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, &task, 0);
}

void setVolume(int percent) { volume = constrain(percent, 0, 100); }

void playChime() {
  if (!task) return;
  playing = true;
  xTaskNotify(task, kChime, eSetValueWithOverwrite);
}

void playBlip() {
  if (!task) return;
  playing = true;
  xTaskNotify(task, kBlip, eSetValueWithOverwrite);
}

void playAlarm() {
  if (!task) return;
  playing = true;
  xTaskNotify(task, kAlarm, eSetValueWithOverwrite);
}

void stop() { abortSound = true; }

bool busy() { return playing; }

} // namespace audio
