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

#include "state/listening.hpp"
#include "app.hpp"
#include "audio/audioinput.hpp"
#include "audio/audioplayer.hpp"
#include "audio/audiovolume.hpp"
#include "leds.hpp"
#include "stt.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::Listening"

namespace genie {
namespace state {

void Listening::enter() {
  State::enter();

  app->leds->animate(LedsState_t::Listening);
  app->stt->begin_session(is_follow_up);
  app->audio_input->wake();
  app->audio_volume_controller->duck();
  g_message("Stopping audio player...\n");
  app->audio_player->stop();
  if (!is_follow_up) {
    g_message("Playing WAKE sound...\n");
    app->audio_player->play_sound(Sound_t::WAKE);
  }
  g_message("Connecting STT...\n");
}

void Listening::react(events::Wake *) {
  g_debug("FIXME Received Wake event while in Listening state.\n");
}

void Listening::react(events::InputFrame *input_frame) {
  app->stt->send_frame(std::move(input_frame->frame));
}

void Listening::react(events::InputDone *input_done) {
  g_message("Handling InputDone...\n");
  app->stt->send_done();
  app->audio_player->stop();
  if (input_done->vad_detected) {
    app->audio_player->play_sound(Sound_t::WORKING);
  }
  app->transit(new Processing(app));
}

void Listening::react(events::InputNotDetected *) {
  g_message("Handling InputNotDetected...\n");
  app->stt->abort();
  app->audio_player->stop();
  app->audio_player->play_sound(Sound_t::NO_INPUT);
  app->transit(new Sleeping(app));
}

void Listening::react(events::InputTimeout *) {
  g_message("Handling InputTimeout...\n");
  app->stt->abort();
  app->audio_player->stop();
  app->audio_player->play_sound(Sound_t::TOO_MUCH_INPUT);
  app->transit(new Sleeping(app));
}

} // namespace state
} // namespace genie
