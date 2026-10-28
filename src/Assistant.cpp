#include "Assistant.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>

#include "AssistantCerts.h"
#include "Config.h"

// The Worker address and device key live in credentials.h (gitignored).
#if __has_include("credentials.h")
#include "credentials.h"
#endif
#ifndef ASSISTANT_URL
#define ASSISTANT_URL ""
#endif
#ifndef ASSISTANT_TOKEN
#define ASSISTANT_TOKEN ""
#endif

namespace assistant {
namespace {

constexpr size_t kAnswerMax = 1200;

struct Turn {
  String question;
  String answer;
};
Turn history[ASSISTANT_MAX_TURNS];
int historyCount = 0;

SemaphoreHandle_t mutex = nullptr;
TaskHandle_t task = nullptr;
volatile State gState = State::Idle;
volatile Error gError = Error::None;
volatile int gStatus = 0;
char gAnswer[kAnswerMax] = "";
// Bumped by newConversation(): a request started before it is ignored.
volatile uint32_t gGeneration = 0;
uint32_t jobGeneration = 0;

// Request body (JSON with the audio as base64), in PSRAM.
uint8_t *body = nullptr;
size_t bodyCap = 0;
size_t bodyLen = 0;
uint8_t *wav = nullptr;
size_t wavCap = 0;

template <typename F> void locked(F f) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  f();
  xSemaphoreGive(mutex);
}

uint8_t *ensure(uint8_t *&buf, size_t &cap, size_t want) {
  if (cap >= want) return buf;
  if (buf) heap_caps_free(buf);
  buf = static_cast<uint8_t *>(heap_caps_malloc(want, MALLOC_CAP_SPIRAM));
  cap = buf ? want : 0;
  return buf;
}

void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = v >> (8 * i); }

// 16-bit mono PCM -> WAV file in `wav`.
size_t buildWav(const int16_t *samples, size_t count, int rate) {
  const size_t data = count * 2;
  if (!ensure(wav, wavCap, data + 44)) return 0;
  memcpy(wav, "RIFF", 4);
  put32(wav + 4, 36 + data);
  memcpy(wav + 8, "WAVEfmt ", 8);
  put32(wav + 16, 16);
  put16(wav + 20, 1);            // PCM
  put16(wav + 22, 1);            // mono
  put32(wav + 24, rate);
  put32(wav + 28, rate * 2);     // bytes per second
  put16(wav + 32, 2);            // block align
  put16(wav + 34, 16);           // bits
  memcpy(wav + 36, "data", 4);
  put32(wav + 40, data);
  memcpy(wav + 44, samples, data);
  return data + 44;
}

void finish(State s, Error e, int status, const char *text = nullptr) {
  locked([&] {
    if (jobGeneration != gGeneration) {
      gState = State::Idle; // conversation was reset meanwhile
      return;
    }
    gError = e;
    gStatus = status;
    if (text) snprintf(gAnswer, sizeof(gAnswer), "%s", text);
    gState = s;
  });
}

void request() {
  if (WiFi.status() != WL_CONNECTED) {
    finish(State::Failed, Error::NoWifi, 0);
    return;
  }
  WiFiClientSecure client;
  client.setCACert(kAssistantRootCAs);
  HTTPClient http;
  if (!http.begin(client, ASSISTANT_URL)) {
    finish(State::Failed, Error::Server, -1);
    return;
  }
  http.setConnectTimeout(10000);
  http.setTimeout(30000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + ASSISTANT_TOKEN);
  const unsigned long t0 = millis();
  const int code = http.POST(body, bodyLen);
  const String reply = code > 0 ? http.getString() : String();
  http.end();
  Serial.printf("[Assistant] HTTP %d in %lu ms (%u bytes sent)\n", code, millis() - t0,
                static_cast<unsigned>(bodyLen));
  if (code == 401 || code == 403) {
    finish(State::Failed, Error::Unauthorized, code);
    return;
  }
  if (code != 200) {
    if (code > 0) Serial.printf("[Assistant] %s\n", reply.substring(0, 200).c_str());
    finish(State::Failed, Error::Server, code);
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, reply)) {
    finish(State::Failed, Error::Server, -2);
    return;
  }
  const char *question = doc["question"] | "";
  const char *ans = doc["answer"] | "";
  if (!*ans) {
    finish(State::Failed, Error::NotHeard, code);
    return;
  }
  locked([&] {
    if (jobGeneration != gGeneration) return;
    if (historyCount == ASSISTANT_MAX_TURNS) {
      for (int i = 1; i < historyCount; i++) history[i - 1] = history[i];
      historyCount--;
    }
    history[historyCount].question = question;
    history[historyCount].answer = ans;
    historyCount++;
  });
  finish(State::Done, Error::None, code, ans);
}

void assistantTask(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    request();
  }
}

} // namespace

void begin() {
  mutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(assistantTask, "assistant", 12288, nullptr, 1, &task, 0);
}

bool configured() { return ASSISTANT_URL[0] != 0 && ASSISTANT_TOKEN[0] != 0; }

bool ask(const int16_t *samples, size_t count, int rate, const char *lang,
         const char *localTime, const char *place) {
  if (!task || gState == State::Busy) return false;
  if (!configured()) {
    gError = Error::NotSetUp;
    gState = State::Failed;
    return true;
  }
  const size_t wavLen = buildWav(samples, count, rate);
  // JSON: everything except the audio, then the audio appended as base64.
  JsonDocument doc;
  doc["lang"] = lang;
  doc["time"] = localTime;
  doc["place"] = place;
  JsonArray h = doc["history"].to<JsonArray>();
  locked([&] {
    for (int i = 0; i < historyCount; i++) {
      JsonObject t = h.add<JsonObject>();
      t["q"] = history[i].question;
      t["a"] = history[i].answer;
    }
  });
  String head;
  serializeJson(doc, head);
  head.remove(head.length() - 1); // drop the closing brace
  head += ",\"audio\":\"";
  const size_t b64 = 4 * ((wavLen + 2) / 3);
  if (!wavLen || !ensure(body, bodyCap, head.length() + b64 + 4)) {
    gError = Error::Server;
    gStatus = -3;
    gState = State::Failed;
    return true;
  }
  memcpy(body, head.c_str(), head.length());
  size_t written = 0;
  mbedtls_base64_encode(body + head.length(), bodyCap - head.length(), &written, wav, wavLen);
  bodyLen = head.length() + written;
  body[bodyLen++] = '"';
  body[bodyLen++] = '}';
  locked([&] {
    jobGeneration = gGeneration;
    gError = Error::None;
    gStatus = 0;
    gState = State::Busy;
  });
  xTaskNotifyGive(task);
  return true;
}

State state() { return gState; }
Error error() { return gError; }
int httpStatus() { return gStatus; }

void answer(char *out, size_t n) {
  locked([&] { snprintf(out, n, "%s", gAnswer); });
}

int turns() {
  int n = 0;
  locked([&] { n = historyCount; });
  return n;
}

void newConversation() {
  locked([&] {
    gGeneration++;
    for (int i = 0; i < historyCount; i++) history[i] = Turn();
    historyCount = 0;
    gAnswer[0] = 0;
    gError = Error::None;
    // A request still running finishes quietly (its result is dropped).
    if (gState != State::Busy) gState = State::Idle;
  });
}

} // namespace assistant
