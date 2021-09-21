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

#include "state/events.hpp"
#include "state/state.hpp"

namespace genie {
namespace state {

class Saying : public State {
public:
  static const constexpr char *NAME = "Saying";

  Saying(App *app, gint64 text_id, const std::string text)
      : State{app}, text_id(text_id), text(text) {}

  void enter() override;

  void react(events::AskSpecialMessage *ask_special_message) override;
  void react(events::PlayerStreamEnter *player_stream_enter) override;
  void react(events::PlayerStreamEnd *player_stream_end) override;

private:
  gint64 text_id;
  std::string text;
  bool follow_up = false;
};

} // namespace state
} // namespace genie
