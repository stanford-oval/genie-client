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
#include "conversation_client.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::State"

namespace genie {
namespace state {

State::State(Machine *machine) : machine(machine), app(machine->app) {}

// Event Handling Methods
// ===========================================================================

void State::react(events::Event *event) {}

void State::react(events::Wake *) {
  // Normally when we wake we start listening. The exception is the Listen
  // state itself.
  machine->transit(new Listening(machine));
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
  app->audio_player->say(text_message->text, text_message->id);
}

void State::react(events::AudioMessage *audio_message) {
  g_message("Received AudioMessage, playing URL: %s\n", audio_message->url);
  app->audio_player->playURI(audio_message->url.c_str(),
                              AudioDestination::MUSIC);
}

void State::react(events::SoundMessage *sound_message) {
  g_message("Received SoundMessage, playing sound ID: %d\n",
            sound_message->sound_id);
  app->audio_player->playSound(sound_message->sound_id,
                                AudioDestination::MUSIC);
}

void State::react(events::AskSpecialMessage *ask_special_message) {}

void State::react(events::SpotifyCredentials *spotify_credentials) {
  app->spotifyd->set_credentials(spotify_credentials->username,
                                   spotify_credentials->access_token);
}

void State::react(events::AdjustVolume *adjust_volume) {
  app->audio_player->adjust_playback_volume(adjust_volume->delta);
}

void State::react(events::TogglePlayback *) {
  g_warning("TODO Playback toggled");
}

void State::react(events::PlayerStreamEnd *player_stream_end) {
  g_message("Received PlayerStreamEnd with type=%d ref_id=%" G_GINT64_FORMAT
            ", ignoring.",
            player_stream_end->type, player_stream_end->ref_id);
}

// Speech-To-Text (STT)
// ---------------------------------------------------------------------------

void State::react(events::stt::TextResponse *response) {
  app->audio_player.get()->clean_queue();
  app->conversation_client.get()->send_command(response->text);
}

void State::react(events::stt::ErrorResponse *response) {
  g_warning("STT completed with an error (code=%d): %s", response->code,
            response->message);
  app->audio_player.get()->playSound(SOUND_NO_MATCH);
  app->audio_player.get()->resume();
}

} // namespace state
} // namespace genie
