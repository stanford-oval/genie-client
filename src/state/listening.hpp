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

#pragma once

#include "events.hpp"
#include "state.hpp"

namespace genie {
namespace state {

class Listening : public State {
public:
  static const constexpr char *NAME = "Listening";

  Listening(App *app) : State{app} {}

  void enter() override;

  void react(events::Wake *) override;
  void react(events::InputFrame *input_frame) override;
  void react(events::InputDone *) override;
  void react(events::InputNotDetected *) override;
  void react(events::InputTimeout *) override;
};

} // namespace state
} // namespace genie
