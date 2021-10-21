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
#include "audio/audioplayer.hpp"
#include "leds.hpp"
#include "ws-protocol/client.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::Processing"

namespace genie {
namespace state {

void Processing::enter() {
  app->track_processing_event(ProcessingEventType::START_STT);
  app->leds->animate(LedsState_t::Processing);
}

void Processing::react(events::TextMessage *text_message) {
  app->track_processing_event(ProcessingEventType::END_GENIE);
  g_message("Received TextMessage, responding with text: %s\n",
            text_message->text.c_str());
  app->transit(new Saying(app, text_message->id, text_message->text));
}

void Processing::react(events::stt::TextResponse *response) {
  app->track_processing_event(ProcessingEventType::END_STT);
  app->audio_player.get()->clean_queue();
  app->track_processing_event(ProcessingEventType::START_GENIE);
  app->conversation_client.get()->send_command(response->text);
}

void Processing::react(events::stt::ErrorResponse *response) {
  app->track_processing_event(ProcessingEventType::END_STT);
  g_warning("STT completed with an error (code=%d): %s", response->code,
            response->message.c_str());
  if (response->code != 404) {
    app->audio_player.get()->play_sound(Sound_t::STT_ERROR);
    app->leds->animate(LedsState_t::Error);
  }
  app->transit(new Sleeping(app));
}

} // namespace state
} // namespace genie
