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

namespace genie {

class App;

namespace state {

class Machine;

class State {
public:
  Machine *machine;
  App *app;

  State(Machine *machine);
  virtual ~State() = default;

  virtual void enter() {};
  virtual void exit() {};

  virtual void react(events::Event *event);
  virtual void react(events::Wake *);
  virtual void react(events::InputFrame *input_frame);
  virtual void react(events::InputDone *);
  virtual void react(events::InputNotDetected *);
  virtual void react(events::InputTimeout *);
  virtual void react(events::TextMessage *text_message);
  virtual void react(events::AudioMessage *audio_message);
  virtual void react(events::SoundMessage *sound_message);
  virtual void react(events::AskSpecialMessage *ask_special_message);
  virtual void react(events::SpotifyCredentials *spotify_credentials);
  virtual void react(events::AdjustVolume *adjust_volume);
};

} // namespace state
} // namespace genie
