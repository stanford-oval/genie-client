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

#include <cstddef>
#include <cstdint>
#include <glib.h>

namespace genie {

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

enum class Sound_t {
  NO_INPUT,
  WAKE,
  NEWS_INTRO,
  ALARM_CLOCK_ELAPSED,
  WORKING,
  STT_ERROR,
  TOO_MUCH_INPUT,
};

enum class AudioTaskType {
  SAY,
  URL,
};

enum class AudioDriverType { ALSA, PULSEAUDIO };

static inline const char *audio_driver_type_to_string(AudioDriverType driver) {
  switch (driver) {
    case AudioDriverType::ALSA:
      return "alsa";
    case AudioDriverType::PULSEAUDIO:
      return "pulseaudio";
    default:
      g_assert_not_reached();
      return "";
  }
}

} // namespace genie
