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

#define G_LOG_DOMAIN "genie::state::events"

namespace genie {
namespace state {
namespace events {

// ### Conversation Events ###

InputFrame::InputFrame(AudioFrame *frame) : frame(frame) {}
InputFrame::~InputFrame() {
  // NOTE We do **NOT** free the frame on destruction -- it's handed off to the
  //      STT instance, where it may be queued for later transmission.
}

TextMessage::TextMessage(const gchar *text) : text(g_strdup(text)) {}
TextMessage::~TextMessage() { g_free(text); }

AudioMessage::AudioMessage(const gchar *url) : url(g_strdup(url)) {}
AudioMessage::~AudioMessage() { g_free(url); }

SoundMessage::SoundMessage(Sound_t id) : id(id) {}

AskSpecialMessage::AskSpecialMessage(const gchar *ask) : ask(g_strdup(ask)) {}
AskSpecialMessage::~AskSpecialMessage() { g_free(ask); }

SpotifyCredentials::SpotifyCredentials(const gchar *username,
                                       const gchar *access_token)
    : username(g_strdup(username)), access_token(g_strdup(access_token)) {}

SpotifyCredentials::~SpotifyCredentials() {
  g_free(access_token);
  g_free(username);
}

// ### Physical Button Events ###

AdjustVolume::AdjustVolume(long delta) : delta(delta) {}

} // namespace events
} // namespace state
} // namespace genie
