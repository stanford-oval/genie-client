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

enum Sound_t {
  SOUND_NO_MATCH = -1,
  SOUND_MATCH = 0,
  SOUND_NEWS_INTRO = 1,
  SOUND_ALARM_CLOCK_ELAPSED = 2,
};

struct AudioFrame {
  int16_t *samples;
  size_t length;

  AudioFrame(size_t len) : samples(new int16_t[len]), length(len) {}
  ~AudioFrame() { delete[] samples; }

  AudioFrame(const AudioFrame &) = delete;
  AudioFrame &operator=(const AudioFrame &) = delete;

  AudioFrame(AudioFrame &&other)
      : samples(other.samples), length(other.length) {
    other.samples = nullptr;
    other.length = 0;
  }
};

namespace state {
namespace events {

struct Event {
  virtual ~Event() = default;
};

struct Wake : Event {};

struct InputFrame : Event {
  AudioFrame *frame;

  InputFrame(AudioFrame *frame) : frame(frame) {}
};

struct InputDone : Event {};

struct InputNotDetected : Event {};

struct InputTimeout : Event {};

// ### Conversation Events ###

struct TextMessage : Event {
  std::string text;

  TextMessage(const gchar *text) : text(text) {}
};

struct AudioMessage : Event {
  std::string url;

  AudioMessage(const gchar *url) : url(url) {}
};

struct SoundMessage : Event {
  Sound_t id;

  SoundMessage(Sound_t id) : id(id) {}
};

struct AskSpecialMessage : Event {
  std::string ask;
  
  // NOTE   Per the spec, the `ask` field may be `null` in the JSON, which
  //        results in the `const gchar *` being `nullptr`. Since it's undefined
  //        behavior to create a `std::string` from a null `char *` we need 
  //        to check for that condition. In this case, we simply use the empty
  //        string in place of `null`.
  AskSpecialMessage(const gchar *ask) : ask(ask == nullptr ? "" : ask) {}
};

struct SpotifyCredentials : Event {
  std::string access_token;
  std::string username;

  SpotifyCredentials(const gchar *username, const gchar *access_token)
      : access_token(access_token == nullptr ? "" : access_token),
        username(username == nullptr ? "" : username) {}
};

// ### Button Events ###

struct AdjustVolume : Event {
  long delta;

  AdjustVolume(long delta) : delta(delta) {}
};

} // namespace events
} // namespace state
} // namespace genie
