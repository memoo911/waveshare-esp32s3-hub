#include "apps/timer/TimerApp.h"

#include <Arduino.h>

#include "Config.h"
#include "core/System.h"
#include "hw/Audio.h"

namespace timer_app {
namespace {

TimerState timerState = TimerState::Idle;
int timerMinutes = 8;                // chosen time, saved in NVS
int timerTotalSec = 0;
unsigned long timerEndMs = 0;        // Running
unsigned long timerLeftMs = 0;       // Paused
unsigned long ringStartMs = 0;       // Ringing
unsigned long nextBeepMs = 0;

int timerRemainingSec() {
  if (timerState == TimerState::Running) {
    const long ms = static_cast<long>(timerEndMs - millis());
    return ms > 0 ? static_cast<int>((ms + 999) / 1000) : 0;
  }
  if (timerState == TimerState::Paused) return static_cast<int>((timerLeftMs + 999) / 1000);
  return 0;
}

void setTimerMinutes(int m) {
  timerMinutes = constrain(m, 1, ui::kTimerMaxMinutes);
}

} // namespace

void begin() {
  timerMinutes = constrain(sys::prefs().getUChar("tmin", 8), 1, ui::kTimerMaxMinutes);
}

void action(ui::TimerAction a, int preset) {
  using ui::TimerAction;
  switch (a) {
  case TimerAction::Minus: setTimerMinutes(timerMinutes - 1); break;
  case TimerAction::Plus: setTimerMinutes(timerMinutes + 1); break;
  case TimerAction::Preset: setTimerMinutes(preset); break;
  case TimerAction::Start:
    timerTotalSec = timerMinutes * 60;
    timerEndMs = millis() + timerTotalSec * 1000UL;
    timerState = TimerState::Running;
    sys::prefs().putUChar("tmin", static_cast<uint8_t>(timerMinutes));
    Serial.printf("[Timer] start %d min\n", timerMinutes);
    break;
  case TimerAction::Pause:
    timerLeftMs = static_cast<unsigned long>(max(0L, static_cast<long>(timerEndMs - millis())));
    timerState = TimerState::Paused;
    Serial.println("[Timer] pause");
    break;
  case TimerAction::Resume:
    timerEndMs = millis() + timerLeftMs;
    timerState = TimerState::Running;
    Serial.println("[Timer] resume");
    break;
  case TimerAction::Reset:
    timerState = TimerState::Idle;
    Serial.println("[Timer] reset");
    break;
  case TimerAction::Stop:
    timerState = TimerState::Idle;
    audio::stop();
    Serial.println("[Timer] stop");
    break;
  case TimerAction::None: break;
  }
}

void update() {
  const unsigned long now = millis();
  if (timerState == TimerState::Running && static_cast<long>(now - timerEndMs) >= 0) {
    Serial.println("[Timer] time's up");
    timerState = TimerState::Ringing;
    ringStartMs = now;
    nextBeepMs = now;
    sys::wakeScreen();
    sys::openPage(Page::Timer);
  }
  if (timerState == TimerState::Ringing) {
    sys::keepAwake(); // keep the screen on while it rings
    if (now - ringStartMs >= TIMER_RING_MS) {
      Serial.println("[Timer] alarm over");
      timerState = TimerState::Idle;
    } else if (static_cast<long>(now - nextBeepMs) >= 0) {
      audio::playAlarm();
      nextBeepMs += TIMER_RING_EVERY_MS;
    }
  }
}

bool ringing() { return timerState == TimerState::Ringing; }

bool active() { return timerState != TimerState::Idle; }

void fill(TimerInfo &out) {
  out.state = timerState;
  out.minutes = timerMinutes;
  out.remainingSec = timerRemainingSec();
  out.totalSec = timerTotalSec;
}

} // namespace timer_app
