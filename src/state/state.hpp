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
#include <chrono>

namespace genie {

class App;

namespace state {

class Machine;

class State {
private:
  std::chrono::steady_clock::time_point enter_time;
  std::chrono::steady_clock::time_point exit_time;

public:
  static const constexpr char *NAME = "State";

  App *app;

  State(App *app) : enter_time(), exit_time(), app(app) {}
  virtual ~State() = default;

  virtual void enter();
  virtual void exit();
  virtual const char *name() = 0;

  // note: you must not define a react for the base Event class
  // otherwise you won't get any error when you add new event classes
  // and forget to declare them here
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
  virtual void react(events::TogglePlayback *);
  virtual void react(events::PlayerStreamEnter *player_stream_enter);
  virtual void react(events::PlayerStreamEnd *player_stream_end);
  virtual void react(events::stt::TextResponse *response);
  virtual void react(events::stt::ErrorResponse *response);
  virtual void react(events::audio::CheckSpotifyEvent *check_spotify);
  virtual void react(events::audio::PrepareEvent *prepare);
  virtual void react(events::audio::PlayURLsEvent *play_urls);
  virtual void react(events::audio::StopEvent *stop);
  virtual void react(events::audio::SetMuteEvent *set_mute);
  virtual void react(events::audio::SetVolumeEvent *set_volume);
  virtual void react(events::audio::AdjVolumeEvent *adj_volume);
};

} // namespace state
} // namespace genie
