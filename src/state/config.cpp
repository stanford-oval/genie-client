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

#include "state/config.hpp"
#include "config.hpp"
#include "utils/net.hpp"
#include "leds.hpp"
#include "app.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::Config"

namespace genie {
namespace state {

void Config::enter() {
  State::enter();

  app->leds->animate(LedsState_t::Config);
  // app->net_controller->stop_sta();
  // app->net_controller->start_ap();
}

void Config::exit() {
  State::exit();
  // app->net_controller->stop_ap();
  // app->net_controller->start_sta();
}

void Config::react(events::ToggleConfigMode *) {
  // exit config state
  app->transit(new Sleeping(app));
}

} // namespace state
} // namespace genie
