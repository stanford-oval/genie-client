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
#include "audioinput.hpp"
#include "audioplayer.hpp"
#include "stt.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::Listening"

namespace genie {
namespace state {

Listening::Listening(Machine *machine) : State{machine} {}

void Listening::enter() {
  g_message("ENTER state Listening\n");
  app->stt->begin_session();
  app->audio_input->wake();
  app->duck();
  g_message("Stopping audio player...\n");
  app->audio_player->stop();
  g_message("Playing match sound...\n");
  app->audio_player->playSound(Sound_t::MATCH);
  g_message("Connecting STT...\n");
}

void Listening::react(events::Wake *) {
  g_warning("FIXME Received Wake event while in Listening state.\n");
}

void Listening::react(events::InputFrame *input_frame) {
  app->stt->send_frame(std::move(input_frame->frame));
}

void Listening::react(events::InputDone *) {
  g_message("Handling InputDone...\n");
  app->stt->send_done();
  app->audio_player->stop();
  app->audio_player->playSound(Sound_t::SENDING);
  machine->transit(new Processing(machine));
}

void Listening::react(events::InputNotDetected *) {
  g_message("Handling InputNotDetected...\n");
  app->stt->abort();
  app->audio_player->stop();
  app->audio_player->playSound(Sound_t::NO_MATCH);
  machine->transit(new Sleeping(machine));
}

void Listening::react(events::InputTimeout *) {
  g_message("Handling InputTimeout...\n");
  app->stt->abort();
  app->audio_player->stop();
  app->audio_player->playSound(Sound_t::NO_MATCH);
  machine->transit(new Sleeping(machine));
}

} // namespace state
} // namespace genie
