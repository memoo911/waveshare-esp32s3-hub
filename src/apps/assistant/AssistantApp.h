#pragma once

// AI Assistant page logic: hold BOOT to record a question, send it (through
// AssistantClient) and show the answer. The conversation lasts until "New
// conversation" or leaving the page. Drawing is in AssistantUi.h.

#include "ui/UiState.h"

namespace assistant_app {

void begin();
// The page was opened / left (leaving ends the conversation).
void enter();
void leave();
// BOOT pressed (down) or released on the page.
void button(bool down);
void newConversation();
// Listening or waiting for an answer: keep the screen on.
bool busy();
// Every frame while the page is open.
void update(AssistantInfo &out);

} // namespace assistant_app
