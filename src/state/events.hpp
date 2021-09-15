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

#include <string>

namespace genie {
namespace state {
namespace events {

struct Event {
  virtual ~Event() = default;
};

// Audio Input Events
// ===========================================================================

struct Wake : Event {};

struct InputFrame : Event {
  AudioFrame frame;

  InputFrame(AudioFrame frame) : frame(std::move(frame)) {}
};

struct InputDone : Event {};

struct InputNotDetected : Event {};

struct InputTimeout : Event {};

// Conversation Events
// ===========================================================================

struct TextMessage : Event {
  gint64 id;
  std::string text;

  TextMessage(gint64 id, const gchar *text) : id(id), text(text) {}
};

struct AudioMessage : Event {
  std::string url;

  AudioMessage(const gchar *url) : url(url) {}
};

struct SoundMessage : Event {
  Sound_t sound_id;

  SoundMessage(Sound_t sound_id) : sound_id(sound_id) {}
};

struct AskSpecialMessage : Event {
  std::string ask;
  gint64 text_id;

  // NOTE   Per the spec, the `ask` field may be `null` in the JSON, which
  //        results in the `const gchar *` being `nullptr`. Since it's undefined
  //        behavior to create a `std::string` from a null `char *` we need
  //        to check for that condition. In this case, we simply use the empty
  //        string in place of `null`.
  AskSpecialMessage(const gchar *ask, gint64 text_id)
      : ask(ask == nullptr ? "" : ask), text_id(text_id) {}
};

struct SpotifyCredentials : Event {
  std::string access_token;
  std::string username;

  SpotifyCredentials(const gchar *username, const gchar *access_token)
      : access_token(access_token == nullptr ? "" : access_token),
        username(username == nullptr ? "" : username) {}
};

// Button Events
// ===========================================================================

struct AdjustVolume : Event {
  long delta;

  AdjustVolume(long delta) : delta(delta) {}
};

// Audio Player Events
// ===========================================================================

struct PlayerStreamEnd : Event {
  AudioTaskType type;
  gint64 ref_id;

  PlayerStreamEnd(AudioTaskType type, gint64 ref_id)
      : type(type), ref_id(ref_id) {}
};

} // namespace events
} // namespace state
} // namespace genie
