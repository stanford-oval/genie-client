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

#include "app.hpp"
#include "audiodriver.hpp"

#include <alsa/asoundlib.h>

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

namespace genie {

class AudioInputAlsa : public AudioInputDriver {
public:
  AudioInputAlsa(App *app);
  ~AudioInputAlsa();
  bool init(gchar *audio_input_device, int sample_rate, int channels,
            int max_frame_length);
  AudioFrame read_frame(int32_t frame_length);

private:
  // initialized once and never overwritten
  App *const app;
  snd_pcm_t *alsa_handle = NULL;

  bool init_pcm(gchar *input_audio_device);
  bool init_speex();

  SpeexEchoState *echo_state;
  SpeexPreprocessState *pp_state;

  int16_t *pcm;
  int16_t *pcm_mono;
  int16_t *pcm_playback;
  int16_t *pcm_filter;
  size_t sample_rate;
  int16_t channels;
  size_t frame_length;
};

} // namespace genie
