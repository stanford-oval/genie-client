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

#include "app.hpp"
#include "stt.hpp"

#define G_LOG_DOMAIN "genie::state::Listening"

namespace genie {
namespace state {
  
Listening::Listening(Machine *machine) : State{machine} {}

void Listening::enter() {
  g_message("ENTER state Listening\n");
  app->duck();
  g_message("Stopping audio player...\n");
  app->m_audioPlayer->stop();
  g_message("Playing match sound...\n");
  app->m_audioPlayer->playSound(SOUND_MATCH);
  g_message("Connecting STT...\n");
  app->m_stt->begin_session();
}
  
void Listening::react(events::Wake *) {
  g_warning("FIXME Received Wake event while in Listening state.\n");
}

void Listening::react(events::InputFrame *input_frame) {
  app->m_stt->send_frame(input_frame->frame);
}

void Listening::react(events::InputDone *) {
  g_message("Handling InputDone...\n");
  app->m_stt->send_done();
  app->m_audioPlayer->stop();
  app->m_audioPlayer->playSound(SOUND_MATCH);
  machine->transit<Sleeping>();
}

void Listening::react(events::InputNotDetected *) {
  g_message("Handling InputNotDetected...\n");
  app->m_stt->abort();
  app->m_audioPlayer->stop();
  app->m_audioPlayer->playSound(SOUND_NO_MATCH);
  machine->transit<Sleeping>();
}

void Listening::react(events::InputTimeout *) {
  g_message("Handling InputTimeout...\n");
  app->m_stt->abort();
  app->m_audioPlayer->stop();
  app->m_audioPlayer->playSound(SOUND_NO_MATCH);
  machine->transit<Sleeping>();
}

} // namespace state
} // namespace genie
