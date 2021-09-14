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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::Sleeping"

namespace genie {
namespace state {

Sleeping::Sleeping(Machine *machine) : State{machine} {}

void Sleeping::enter() {
  g_message("ENTER state Sleeping\n");
  this->app->unduck();
}

} // namespace state
} // namespace genie
