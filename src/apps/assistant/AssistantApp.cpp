#include "apps/assistant/AssistantApp.h"

#include <Arduino.h>
#include <time.h>

#include "Config.h"
#include "apps/assistant/AssistantClient.h"
#include "core/System.h"
#include "hw/Audio.h"
#include "net/Net.h"

namespace assistant_app {
namespace {

AssistantPhase aiPhase = AssistantPhase::Ready;
AssistantError aiError = AssistantError::None;
int aiStatus = 0;
unsigned long aiRecordStartMs = 0;
char aiAnswer[1200] = "";

void assistantFail(AssistantError e, int status = 0) {
  aiPhase = AssistantPhase::Error;
  aiError = e;
  aiStatus = status;
}

} // namespace

void begin() { assistant::begin(); }

bool busy() { return aiPhase == AssistantPhase::Listening || aiPhase == AssistantPhase::Thinking; }

void enter() {
  aiPhase = AssistantPhase::Ready;
  aiAnswer[0] = 0;
  if (!assistant::configured()) assistantFail(AssistantError::NotSetUp);
}

void leave() {
  audio::stopRecording();
  assistant::newConversation();
  aiPhase = AssistantPhase::Ready;
  aiAnswer[0] = 0;
}

void newConversation() {
  Serial.println("[Assistant] new conversation");
  assistant::newConversation();
  aiPhase = AssistantPhase::Ready;
  aiAnswer[0] = 0;
}

// BOOT pressed / released on the assistant page.
void button(bool down) {
  if (down) {
    if (aiPhase == AssistantPhase::Thinking || aiPhase == AssistantPhase::Listening) return;
    if (!assistant::configured()) {
      assistantFail(AssistantError::NotSetUp);
      return;
    }
    if (audio::startRecording(ASSISTANT_MAX_RECORD_MS)) {
      aiPhase = AssistantPhase::Listening;
      aiRecordStartMs = millis();
      Serial.println("[Assistant] listening");
    }
  } else if (aiPhase == AssistantPhase::Listening) {
    audio::stopRecording();
  }
}

void update(AssistantInfo &a) {
  if (aiPhase == AssistantPhase::Listening && !audio::recording()) {
    size_t n = 0;
    const int16_t *samples = audio::recordedSamples(n);
    if (n * 1000 / audio::kMicRate < ASSISTANT_MIN_RECORD_MS) {
      assistantFail(AssistantError::TooShort);
    } else {
      // Local time and place give Gemini context ("what day is it", "will it rain").
      char when[48] = "";
      time_t now = time(nullptr);
      struct tm t;
      localtime_r(&now, &t);
      if (t.tm_year > 120) strftime(when, sizeof(when), "%Y-%m-%d %H:%M (%A)", &t);
      const Place place = net::place();
      if (assistant::ask(samples, n, audio::kMicRate, sys::language() == Lang::Hr ? "hr" : "en", when,
                         place.name)) {
        aiPhase = AssistantPhase::Thinking;
      } else {
        assistantFail(AssistantError::Server, 0); // previous request still running
      }
    }
  }
  if (aiPhase == AssistantPhase::Thinking) {
    switch (assistant::state()) {
    case assistant::State::Done:
      assistant::answer(aiAnswer, sizeof(aiAnswer));
      aiPhase = AssistantPhase::Answer;
      break;
    case assistant::State::Failed:
      switch (assistant::error()) {
      case assistant::Error::NotSetUp: assistantFail(AssistantError::NotSetUp); break;
      case assistant::Error::NoWifi: assistantFail(AssistantError::NoWifi); break;
      case assistant::Error::Unauthorized: assistantFail(AssistantError::Unauthorized); break;
      case assistant::Error::NotHeard: assistantFail(AssistantError::NotHeard); break;
      default: assistantFail(AssistantError::Server, assistant::httpStatus()); break;
      }
      break;
    case assistant::State::Idle: // conversation was reset meanwhile
      aiPhase = AssistantPhase::Ready;
      break;
    case assistant::State::Busy: break;
    }
  }
  a.phase = aiPhase;
  a.error = aiError;
  a.status = aiStatus;
  a.turns = assistant::turns();
  a.recordMs = aiPhase == AssistantPhase::Listening ? static_cast<int>(millis() - aiRecordStartMs) : 0;
  a.level = audio::micLevel();
  a.answer = aiAnswer;
}

} // namespace assistant_app
