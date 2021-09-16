// -*- mode: cpp; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// This file is part of Genie
//
// Copyright 2021 The Board of Trustees of the Leland Stanford Junior University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "state/processing.hpp"
#include "app.hpp"
#include "audioplayer.hpp"
#include "conversation_client.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::Processing"

namespace genie {
namespace state {

void Processing::react(events::TextMessage *text_message) {
  g_message("Received TextMessage, responding with text: %s\n",
            text_message->text.c_str());
  machine->transit(new Saying(machine, text_message->id, text_message->text));
}

void Processing::react(events::stt::TextResponse *response) {
  app->audio_player.get()->clean_queue();
  app->conversation_client.get()->send_command(response->text);
}

void Processing::react(events::stt::ErrorResponse *response) {
  g_warning("STT completed with an error (code=%d): %s", response->code,
            response->message);
  app->audio_player.get()->playSound(Sound_t::NO_MATCH);
  app->audio_player.get()->resume();
  machine->transit(new Sleeping(machine));
}

} // namespace state
} // namespace genie
