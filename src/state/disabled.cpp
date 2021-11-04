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

#include "state/disabled.hpp"
#include "app.hpp"
#include "audio/audioplayer.hpp"
#include "audio/audiovolume.hpp"
#include "leds.hpp"
#include "state/sleeping.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::Disabled"

namespace genie {
namespace state {

void Disabled::enter() {
  State::enter();
  app->leds->animate(LedsState_t::Disabled);
}

void Disabled::exit() { State::exit(); }

void Disabled::react(events::ToggleDisabled *) {
  g_message("ENABLING....");
  app->transit(new Sleeping(app));
}

} // namespace state
} // namespace genie
