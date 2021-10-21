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

#include "state/sleeping.hpp"
#include "app.hpp"
#include "audio/audioplayer.hpp"
#include "audio/audiovolume.hpp"
#include "leds.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::Sleeping"

namespace genie {
namespace state {

void Sleeping::enter() {
  g_message("ENTER state Sleeping\n");
  app->audio_volume_controller->unduck();
  app->leds->animate(LedsState_t::Sleeping);
}

void Sleeping::react(events::audio::PrepareEvent *prepare) {
  app->audio_player->clean_queue();
  prepare->resolve();
}

void Sleeping::react(events::audio::PlayURLsEvent *play_urls) {
  g_message("Reacting to PlayURLsEvent in sleeping state, about to play...");

  app->audio_player->clean_queue();

  for (const auto &url : play_urls->urls)
    app->audio_player->play_url(url);

  play_urls->resolve();
}

} // namespace state
} // namespace genie
