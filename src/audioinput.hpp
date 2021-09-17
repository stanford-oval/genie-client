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
#include <glib.h>
#include <webrtc/webrtc_vad.h>
#include <queue>
#include <atomic>

#include <alsa/asoundlib.h>
#include <pulse/simple.h>
#include <pulse/error.h>

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
    WAITING,
    WOKE,
    LISTENING,
  };

  AudioInput(App *appInstance);
  ~AudioInput();
  int init();
  void close();
  void wake();

protected:
  static void *loop(gpointer data);

private:
  bool running;
  snd_pcm_t *alsa_handle = NULL;
  pa_simple *pulse_handle = NULL;

  void *porcupine_library;
  pv_porcupine_t *porcupine;
  void (*pv_porcupine_delete_func)(pv_porcupine_t *);
  pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *,
                                           int32_t *);
  const char *(*pv_status_to_string_func)(pv_status_t);

  int16_t *pcm;
  int16_t *pcmOutput;
  int16_t *pcmFilter;
  int32_t pv_frame_length;
  int32_t sample_rate;
  std::queue<AudioFrame> frame_buffer;

  SpeexEchoState *echo_state;
  SpeexPreprocessState *pp_state;

  VadInst *vad_instance;
  App *app;

  int32_t vad_start_frame_count;
  int32_t vad_done_frame_count;

  // How many frames we need to be woke before we go into `State::LISTENING`,
  // where we terminate input after the `vad_done_frame_count`
  int32_t min_woke_frame_count;

  // Loop state variables
  std::atomic<State> state;
  int32_t state_woke_frame_count;
  int32_t state_vad_frame_count;

  AudioFrame read_frame(int32_t frame_length);

  int32_t ms_to_frames(int32_t frame_length, int32_t ms);
  bool init_pv();
  bool init_alsa(gchar *input_audio_device);
  bool init_pulse();
  void loop_waiting();
  void loop_woke();
  void loop_listening();
  void transition(State to_state);
};

} // namespace genie
