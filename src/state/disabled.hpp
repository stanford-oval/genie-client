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

class Disabled : public State {
public:
  static const constexpr char *NAME = "Disabled";

  Disabled(App *app) : State{app} {}

  void enter() override;
  void exit() override;
  const char *name() override { return NAME; };

  // The "actual" handlers that do things
  void react(events::ToggleDisabled *) override;
  void react(events::Panic *) override;

  // Events that state::State handles that we want to ignore
  void react(events::Wake *) override {}
  void react(events::TextMessage *) override {}
  void react(events::AudioMessage *) override {}
  void react(events::SoundMessage *) override {}
  void react(events::AdjustVolume *) override {}
  void react(events::PlayerStreamEnter *) override {}
  void react(events::PlayerStreamEnd *) override {}

  // Audio Control Protocol events that we want to just resolve "so the server
  // doesn't block on the client" (- Gio)
  void react(events::audio::PrepareEvent *event) override { event->resolve(); }
  void react(events::audio::PlayURLsEvent *event) override { event->resolve(); }
  void react(events::audio::StopEvent *event) override { event->resolve(); }
  void react(events::audio::SetMuteEvent *event) override { event->resolve(); }
  void react(events::audio::SetVolumeEvent *event) override {
    event->resolve();
  }
  void react(events::audio::AdjVolumeEvent *event) override {
    event->resolve();
  }
};

} // namespace state
} // namespace genie
