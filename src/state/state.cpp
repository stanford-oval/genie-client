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

#include "state/state.hpp"
#include "app.hpp"
#include "audio/audioplayer.hpp"
#include "audio/audiovolume.hpp"
#include "spotifyd.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::state::State"

namespace genie {
namespace state {

void State::enter() {
  g_message("ENTER state %s\n", name());
  enter_time = std::chrono::steady_clock::now();
}

void State::exit() {
  exit_time = std::chrono::steady_clock::now();

  auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
      exit_time - enter_time);
  g_message("Spent %ld milliseconds in state %s", (long)delta.count(), name());
}

// Event Handling Methods
// ===========================================================================

void State::react(events::Wake *) {
  // Normally when we wake we start listening. The exception is the Listen
  // state itself.
  app->transit(new Listening(app));
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

// Genie Server Message Events
// ---------------------------------------------------------------------------
//
// Dispatched when `genie::conversation::ConversationProtocol` receives a
// message on the websocket from the Genie server.
//

void State::react(events::TextMessage *text_message) {
  g_message("Received TextMessage, saying text: %s\n",
            text_message->text.c_str());
  app->transit(new Saying(app, text_message->id, text_message->text));
}

void State::react(events::AudioMessage *audio_message) {
  // TODO this should be deferred to the sleeping state

  g_message("Received AudioMessage, playing URL: %s\n",
            audio_message->url.c_str());
  app->audio_player->play_url(audio_message->url.c_str(),
                              AudioDestination::MUSIC);
}

void State::react(events::SoundMessage *sound_message) {
  // TODO this should look at the "exclusive" flag, and
  // either handle it immediately (queued with other text)
  // or defer to the sleeping state (queued with other music)

  g_message("Received SoundMessage, playing sound ID: %d\n",
            (int)sound_message->sound_id);
  app->audio_player->play_sound(sound_message->sound_id,
                                AudioDestination::ALERT);
}

void State::react(events::AskSpecialMessage *ask_special_message) {}

void State::react(events::SpotifyCredentials *spotify_credentials) {
  app->spotifyd->set_credentials(spotify_credentials->username,
                                 spotify_credentials->access_token);
}

void State::react(events::AdjustVolume *adjust_volume) {
  app->audio_volume_controller->adjust_playback_volume(adjust_volume->delta);
}

void State::react(events::TogglePlayback *) {
  g_warning("TODO Playback toggled in state %s", NAME);
}

void State::react(events::PlayerStreamEnter *player_stream_enter) {
  g_message("Received PlayerStreamEnter with type=%d ref_id=%" G_GINT64_FORMAT
            ", ignoring.",
            (int)player_stream_enter->type, player_stream_enter->ref_id);
}

void State::react(events::PlayerStreamEnd *player_stream_end) {
  g_message("Received PlayerStreamEnd with type=%d ref_id=%" G_GINT64_FORMAT
            ", ignoring.",
            (int)player_stream_end->type, player_stream_end->ref_id);
}

// Speech-To-Text (STT)
// ---------------------------------------------------------------------------

void State::react(events::stt::TextResponse *response) {
  g_warning("FIXME Received events::stt::TextResponse in state %s", NAME);
}

void State::react(events::stt::ErrorResponse *response) {
  g_warning("FIXME Received events::stt::ErrorResponse in state %s", NAME);
}

// Audio Control Protocol

void State::react(events::audio::CheckSpotifyEvent *check_spotify) {
  app->spotifyd->set_credentials(check_spotify->username,
                                 check_spotify->access_token);
  check_spotify->resolve(std::make_pair(true, ""));
}

void State::react(events::audio::PrepareEvent *prepare) {
  // defer this event to the next state
  app->defer(prepare);
}

void State::react(events::audio::PlayURLsEvent *play_urls) {
  g_message("Reacting to PlayURLsEvent in %s state, deferring", NAME);

  // defer this event to the next state
  app->defer(play_urls);
}

void State::react(events::audio::StopEvent *stop) {
  app->audio_player->stop();
  stop->resolve();
}

void State::react(events::audio::SetMuteEvent *set_mute) {
  // TODO implement
  set_mute->resolve();
}

void State::react(events::audio::SetVolumeEvent *set_volume) {
  app->audio_volume_controller->set_volume(set_volume->volume);
  set_volume->resolve();
}

void State::react(events::audio::AdjVolumeEvent *adj_volume) {
  app->audio_volume_controller->adjust_playback_volume(adj_volume->delta);
  adj_volume->resolve();
}

} // namespace state
} // namespace genie
