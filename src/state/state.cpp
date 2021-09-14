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
#include "audioplayer.hpp"
#include "spotifyd.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::State"

namespace genie {
namespace state {

State::State(Machine *machine) : machine(machine), app(machine->app) {}

void State::react(events::Event *event) {}

void State::react(events::Wake *) {
  // Normally when we wake we start listening. The exception is the Listen
  // state itself.
  machine->transit<Listening>();
}

void State::react(events::InputFrame *input_frame) {
  g_warning("FIXME received InputFrame when not in Listen state, discarding.");
}

void State::react(events::InputDone *) {
  g_warning("FIXME received InputDone when not in Listen state, ignoring.");
}

void State::react(events::InputNotDetected *) {
  g_warning("FIXME received InputNotDetected when not in Listen state, "
            "ignoring.");
}

void State::react(events::InputTimeout *) {
  g_warning("FIXME received InputTimeout when not in Listen state, ignoring.");
}

void State::react(events::TextMessage *text_message) {
  g_message("Received TextMessage, saying text: %s\n",
            text_message->text.c_str());
  app->m_audioPlayer->say(text_message->text);
}

void State::react(events::AudioMessage *audio_message) {
  g_message("Received AudioMessage, playing URL: %s\n", audio_message->url);
  app->m_audioPlayer->playURI(audio_message->url.c_str(),
                              AudioDestination::MUSIC);
}

void State::react(events::SoundMessage *sound_message) {
  g_message("Received SoundMessage, playing sound ID: %d\n", sound_message->id);
  app->m_audioPlayer->playSound(sound_message->id, AudioDestination::MUSIC);
}

void State::react(events::AskSpecialMessage *ask_special_message) {
  if (ask_special_message->ask.empty()) {
    g_message("Received empty AskSpecialMessage, round done.");
  } else {
    g_warning("FIXME received AskSpecialMessage with ask=%s, ignoring.",
              ask_special_message->ask.c_str());
  }
}

void State::react(events::SpotifyCredentials *spotify_credentials) {
  app->m_spotifyd->set_credentials(spotify_credentials->username,
                                   spotify_credentials->access_token);
}

void State::react(events::AdjustVolume *adjust_volume) {
  app->m_audioPlayer->adjust_playback_volume(adjust_volume->delta);
}

} // namespace state
} // namespace genie
