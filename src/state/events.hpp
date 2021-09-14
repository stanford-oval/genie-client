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

struct Event {};

struct Wake : Event {};

struct InputFrame : Event {
  AudioFrame *frame;

  InputFrame(AudioFrame *frame);
  ~InputFrame();
};

struct InputDone : Event {};

struct InputNotDetected : Event {};

struct InputTimeout : Event {};

// ### Conversation Events ###

struct TextMessage : Event {
  gchar *text;

  TextMessage(const gchar *text);
  ~TextMessage();
};

struct AudioMessage : Event {
  gchar *url;

  AudioMessage(const gchar *url);
  ~AudioMessage();
};

struct SoundMessage : Event {
  Sound_t id;

  SoundMessage(Sound_t id);
};

struct AskSpecialMessage : Event {
  gchar *ask;

  AskSpecialMessage(const gchar *ask);
  ~AskSpecialMessage();
};

struct SpotifyCredentials : Event {
  gchar *access_token;
  gchar *username;

  SpotifyCredentials(const gchar *username, const gchar *access_token);
  ~SpotifyCredentials();
};

// ### Button Events ###

struct AdjustVolume : Event {
  long delta;

  AdjustVolume(long delta);
};

} // namespace events
} // namespace state
} // namespace genie
