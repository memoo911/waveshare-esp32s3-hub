#pragma once

// Voice assistant: a recorded question goes to your own Cloudflare Worker
// (see worker/ and the README), which asks Gemini and returns a short text
// answer. The Worker keeps the Gemini API key; the board only knows the
// Worker's address and a device key, both in credentials.h (gitignored).
//
// The conversation (questions as Gemini understood them, and its answers)
// is kept here and sent with every question, until newConversation().
// Requests run on their own task, so the UI never waits.

#include <Arduino.h>

namespace assistant {

enum class State : uint8_t { Idle, Busy, Done, Failed };

enum class Error : uint8_t {
  None,
  NotSetUp,     // ASSISTANT_URL / ASSISTANT_TOKEN missing in credentials.h
  NoWifi,
  Unauthorized, // the Worker rejected the device key
  Server,       // Worker or Gemini error, or no connection (see httpStatus)
  NotHeard,     // Gemini returned no answer
};

void begin();

// Address and device key are filled in.
bool configured();

// Sends the recording (mono 16-bit at `rate`) with the conversation so far.
// `lang` ("en"/"hr") is the fallback answer language, `localTime` and
// `place` give Gemini context ("what day is it", "will it rain"). Returns
// false if a request is already running.
bool ask(const int16_t *samples, size_t count, int rate, const char *lang,
         const char *localTime, const char *place);

State state();
Error error();
int httpStatus();
// The last answer (UTF-8).
void answer(char *out, size_t n);
// Questions answered in this conversation.
int turns();
// Forget the conversation (also cancels showing a running request's answer).
void newConversation();

} // namespace assistant
