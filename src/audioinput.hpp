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
#include "audioplayer.hpp"
#include "pv_porcupine.h"
#include "stt.hpp"
#include "utils/webrtc_vad.h"
#include <atomic>
#include <glib.h>
#include <queue>
#include <thread>

#include <alsa/asoundlib.h>
#include <pulse/error.h>
#include <pulse/simple.h>

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#define AUDIO_INPUT_VAD_FRAME_LENGTH 480

namespace genie {

class AudioInput {
public:
  static const int32_t BUFFER_MAX_FRAMES = 32;
  // static const int32_t VAD_FRAME_LENGTH = 480;
  static const int VAD_IS_SILENT = 0;
  static const int VAD_NOT_SILENT = 1;

  enum class State {
    CLOSED,
    WAITING,
    WOKE,
    LISTENING,
  };

  AudioInput(App *appInstance);
  ~AudioInput();
  int init();
  void close();
  void wake();

private:
  // initialized once and never overwritten
  VadInst *const vad_instance;
  App *const app;
  snd_pcm_t *alsa_handle = NULL;
  pa_simple *pulse_handle = NULL;

  void *porcupine_library;
  pv_porcupine_t *porcupine;
  void (*pv_porcupine_delete_func)(pv_porcupine_t *);
  pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *,
                                           int32_t *);
  const char *(*pv_status_to_string_func)(pv_status_t);

  // thread safe, accessed from both threads
  std::thread input_thread;
  std::atomic<State> state;

  // only accessed from the input thread
  int16_t *pcm;
  int16_t *pcm_mono;
  int16_t *pcm_playback;
  int16_t *pcm_filter;
  int32_t pv_frame_length;
  size_t sample_rate;
  int16_t channels;
  std::queue<AudioFrame> frame_buffer;

  SpeexEchoState *echo_state;
  SpeexPreprocessState *pp_state;

  size_t vad_start_frame_count;
  size_t vad_done_frame_count;
  size_t vad_input_detected_noise_frame_count;

  // Loop state variables
  size_t state_woke_frame_count;
  size_t state_vad_silent_count;
  size_t state_vad_noise_count;

  // only called from the input thread
  AudioFrame read_frame(int32_t frame_length);

  size_t ms_to_frames(size_t frame_length, size_t ms);
  bool init_pv();
  bool init_alsa(gchar *input_audio_device, int channels);
  bool init_pulse();
  bool init_speex();
  void loop();
  void loop_waiting();
  void loop_woke();
  void loop_listening();
  void transition(State to_state);
};

} // namespace genie
