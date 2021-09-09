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

#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>

#include "audioinput.hpp"
#include "audiofifo.hpp"
#include "pv_porcupine.h"

#define DEBUG_DUMP_STREAMS

#ifdef DEBUG_DUMP_STREAMS
FILE *fp_input;
FILE *fp_output;
FILE *fp_filter;
#endif

genie::AudioInput::AudioInput(App *appInstance) {
  app = appInstance;
  vad_instance = WebRtcVad_Create();
  frame_buffer = g_queue_new();
}

genie::AudioInput::~AudioInput() {
  running = false;
  WebRtcVad_Free(vad_instance);
  while (!g_queue_is_empty(frame_buffer)) {
    g_free(g_queue_pop_head(frame_buffer));
  }
  g_queue_free(frame_buffer);
  free(pcm);
  snd_pcm_close(alsa_handle);
  pv_porcupine_delete_func(porcupine);
  dlclose(porcupine_library);
#ifdef DEBUG_DUMP_STREAMS
  fclose(fp_input);
  fclose(fp_output);
  fclose(fp_filter);
#endif
}

int genie::AudioInput::init() {
  gchar *input_audio_device = app->m_config->audioInputDevice;

  const char *library_path = "assets/libpv_porcupine.so";
  const char *model_path = "assets/porcupine_params.pv";
  const char *keyword_path = "assets/keyword.ppn";
  const float sensitivity = (float)atof("0.7");

  if (!input_audio_device) {
    g_error("no input audio device");
    return -1;
  }

  porcupine_library = dlopen(library_path, RTLD_NOW);
  if (!porcupine_library) {
    g_error("failed to open library\n");
    return -1;
  }

  char *error = NULL;

  pv_status_to_string_func = (const char *(*)(pv_status_t))dlsym(
      porcupine_library, "pv_status_to_string");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_status_to_string' with '%s'.\n", error);
    return -2;
  }

  int32_t (*pv_sample_rate_func)() =
      (int32_t(*)())dlsym(porcupine_library, "pv_sample_rate");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_sample_rate' with '%s'.\n", error);
    return -2;
  }

  pv_status_t (*pv_porcupine_init_func)(const char *, int32_t,
                                        const char *const *, const float *,
                                        pv_porcupine_t **) =
      (pv_status_t(*)(const char *, int32_t, const char *const *, const float *,
                      pv_porcupine_t **))dlsym(porcupine_library,
                                               "pv_porcupine_init");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_porcupine_init' with '%s'.\n", error);
    return -2;
  }

  pv_porcupine_delete_func = (void (*)(pv_porcupine_t *))dlsym(
      porcupine_library, "pv_porcupine_delete");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_porcupine_delete' with '%s'.\n", error);
    return -2;
  }

  pv_porcupine_process_func =
      (pv_status_t(*)(pv_porcupine_t *, const int16_t *, int32_t *))dlsym(
          porcupine_library, "pv_porcupine_process");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_porcupine_process' with '%s'.\n", error);
    return -2;
  }

  int32_t (*pv_porcupine_frame_length_func)() =
      (int32_t(*)())dlsym(porcupine_library, "pv_porcupine_frame_length");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_porcupine_frame_length' with '%s'.\n", error);
    return -2;
  }

  porcupine = NULL;
  pv_status_t status = pv_porcupine_init_func(model_path, 1, &keyword_path,
                                              &sensitivity, &porcupine);
  if (status != PV_STATUS_SUCCESS) {
    g_error("'pv_porcupine_init' failed with '%s'\n",
            pv_status_to_string_func(status));
    return -2;
  }

  alsa_handle = NULL;
  int error_code =
      snd_pcm_open(&alsa_handle, input_audio_device, SND_PCM_STREAM_CAPTURE, 0);
  if (error_code != 0) {
    g_error("'snd_pcm_open' failed with '%s'\n", snd_strerror(error_code));
    return -2;
  }

  snd_pcm_hw_params_t *hardware_params = NULL;
  error_code = snd_pcm_hw_params_malloc(&hardware_params);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_malloc' failed with '%s'\n",
            snd_strerror(error_code));
    return -2;
  }

  error_code = snd_pcm_hw_params_any(alsa_handle, hardware_params);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_any' failed with '%s'\n",
            snd_strerror(error_code));
    return -2;
  }

  error_code = snd_pcm_hw_params_set_access(alsa_handle, hardware_params,
                                            SND_PCM_ACCESS_RW_INTERLEAVED);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_set_access' failed with '%s'\n",
            snd_strerror(error_code));
    return -2;
  }

  error_code = snd_pcm_hw_params_set_format(alsa_handle, hardware_params,
                                            SND_PCM_FORMAT_S16_LE);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_set_format' failed with '%s'\n",
            snd_strerror(error_code));
    return -2;
  }

  sample_rate = pv_sample_rate_func();

  error_code =
      snd_pcm_hw_params_set_rate(alsa_handle, hardware_params, sample_rate, 0);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_set_rate' failed with '%s'\n",
            snd_strerror(error_code));
    return -2;
  }

  error_code = snd_pcm_hw_params_set_channels(alsa_handle, hardware_params, 1);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_set_channels' failed with '%s'\n",
            snd_strerror(error_code));
    return -2;
  }

  error_code = snd_pcm_hw_params(alsa_handle, hardware_params);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params' failed with '%s'\n", snd_strerror(error_code));
    return -2;
  }

  snd_pcm_hw_params_free(hardware_params);

  error_code = snd_pcm_prepare(alsa_handle);
  if (error_code != 0) {
    g_error("'snd_pcm_prepare' failed with '%s'\n", snd_strerror(error_code));
    return -2;
  }

  pv_frame_length = pv_porcupine_frame_length_func();

  pcm = (int16_t *)malloc(
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, pv_frame_length) *
      sizeof(int16_t));
  if (!pcm) {
    g_error("failed to allocate memory for audio buffer\n");
    return -2;
  }

  pcmOutput = (int16_t *)malloc(
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, pv_frame_length) *
      sizeof(int16_t));
  if (!pcmOutput) {
    g_error("failed to allocate memory for audio buffer\n");
    return -2;
  }

  pcmFilter = (int16_t *)malloc(
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, pv_frame_length) *
      sizeof(int16_t));
  if (!pcmFilter) {
    g_error("failed to allocate memory for audio buffer\n");
    return -2;
  }

  g_print("Initialized wakeword engine, frame length %d, sample rate %d\n",
          pv_frame_length, sample_rate);

  if (WebRtcVad_Init(vad_instance)) {
    g_error("failed to initialize webrtc vad\n");
    return -2;
  }

  int vadMode = 3;
  if (WebRtcVad_set_mode(vad_instance, vadMode)) {
    g_error("unable to set vad mode to %d", vadMode);
    return -2;
  }

  if (WebRtcVad_ValidRateAndFrameLength(sample_rate, pv_frame_length)) {
    g_debug("invalid rate %d or framelength %d", sample_rate, pv_frame_length);
  }

  vad_start_frame_count = ms_to_frames(AUDIO_INPUT_VAD_FRAME_LENGTH,
                                       app->m_config->vad_start_speaking_ms);
  g_message("Calculated start VAD: %d ms -> %d frames",
            app->m_config->vad_start_speaking_ms, vad_start_frame_count);

  vad_done_frame_count = ms_to_frames(AUDIO_INPUT_VAD_FRAME_LENGTH,
                                      app->m_config->vad_done_speaking_ms);
  g_message("Calculated done VAD: %d ms -> %d frames",
            app->m_config->vad_done_speaking_ms, vad_done_frame_count);

  min_woke_frame_count = ms_to_frames(AUDIO_INPUT_VAD_FRAME_LENGTH,
                                      app->m_config->vad_min_woke_ms);
  g_message("Calculated min woke frames: %d ms -> %d frames",
            app->m_config->vad_min_woke_ms, min_woke_frame_count);

  echo_state = speex_echo_state_init_mc(
      pv_frame_length,
      ms_to_frames(AUDIO_INPUT_VAD_FRAME_LENGTH, 500), 1, 1);
  speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &(sample_rate));

  PaUtil_AdvanceRingBufferReadIndex(&app->m_audioFIFO.get()->ring_buffer, PaUtil_GetRingBufferReadAvailable(&app->m_audioFIFO.get()->ring_buffer));

#ifdef DEBUG_DUMP_STREAMS
  fp_input = fopen("/tmp/input.raw", "wb+");
  fp_output = fopen("/tmp/playback.raw", "wb+");
  fp_filter = fopen("/tmp/filter.raw", "wb+");
#endif

  GError *thread_error = NULL;
  g_thread_try_new("audioInputThread", (GThreadFunc)loop, this, &thread_error);
  if (thread_error) {
    g_print("audioInputThread Error: g_thread_try_new() %s\n",
            thread_error->message);
    g_error_free(thread_error);
    return false;
  }
  running = true;

  return 0;
}

/**
 * @brief Convert `ms` milliseconds to number of frames at a given
 * `frame_length` (in samples).
 *
 * Uses `this->sample_rate` to calculate how samples per second.
 *
 * @param frame_length
 * @param ms
 * @return int32_t
 */
int32_t genie::AudioInput::ms_to_frames(int32_t frame_length, int32_t ms) {
  //     floor <-  (samples/sec * (     seconds     )) / (samples/frame)
  return (int32_t)((sample_rate * ((double)ms / 1000)) / frame_length);
}

genie::AudioFrame *genie::AudioInput::read_frame(int32_t frame_length) {
  const int read_frames = snd_pcm_readi(alsa_handle, pcm, frame_length);

  if (read_frames < 0) {
    g_critical("'snd_pcm_readi' failed with '%s'\n", snd_strerror(read_frames));
    return NULL;
  }

  if (read_frames != frame_length) {
    g_message("read %d frames instead of %d\n", read_frames, frame_length);
    return NULL;
  }

  AudioFrame *frame = new AudioFrame(frame_length);

#ifdef DEBUG_DUMP_STREAMS
  fwrite(pcm, sizeof(int16_t), frame_length, fp_input);
#endif
  if (app->m_audioFIFO->isReading()) {
    int timeout = 128;
    while (PaUtil_GetRingBufferReadAvailable(
               &app->m_audioFIFO.get()->ring_buffer) < frame_length &&
           timeout > 0) {
      usleep(1);
      timeout--;
    }
    PaUtil_ReadRingBuffer(&app->m_audioFIFO.get()->ring_buffer, pcmOutput,
                          frame_length);
    speex_echo_cancellation(echo_state, (const spx_int16_t *)pcm,
                            (const int16_t *)pcmOutput,
                            (spx_int16_t *)pcmFilter);
    memcpy(frame->samples, pcmFilter, frame_length * sizeof(int16_t));
#ifdef DEBUG_DUMP_STREAMS
    fwrite(pcmOutput, sizeof(int16_t), frame_length, fp_output);
    fwrite(pcmFilter, sizeof(int16_t), frame_length, fp_filter);
#endif
  } else {
    memcpy(frame->samples, pcm, frame_length * sizeof(int16_t));
  }
  return frame;
}

void genie::AudioInput::transition(State to_state) {
  // Reset state variables
  state_woke_frame_count = 0;
  state_vad_frame_count = 0;

  switch (to_state) {
  case State::WAITING:
    g_message("[AudioInput] -> State::WAITING\n");
    state = State::WAITING;
    break;
  case State::WOKE:
    g_message("[AudioInput] -> State::WOKE\n");
    state = State::WOKE;
    break;
  case State::LISTENING:
    g_message("[AudioInput] -> State::LISTENING\n");
    state = State::LISTENING;
    break;
  }
}

void genie::AudioInput::loop_waiting() {
  AudioFrame *new_frame = read_frame(pv_frame_length);

  if (!new_frame) {
    return;
  }

  // Drop from the queue if there are too many items
  while (g_queue_get_length(frame_buffer) > BUFFER_MAX_FRAMES) {
    AudioFrame *dropped_frame = (AudioFrame *)g_queue_pop_head(frame_buffer);
    g_free(dropped_frame->samples);
    g_free(dropped_frame);
  }

  // Add the new frame to the queue
  g_queue_push_tail(frame_buffer, new_frame);

  // Check the new frame for the wake-word
  int32_t keyword_index = -1;
  pv_status_t status =
      pv_porcupine_process_func(porcupine, new_frame->samples, &keyword_index);

  if (status != PV_STATUS_SUCCESS) {
    // Picovoice error!
    g_critical("'pv_porcupine_process' failed with '%s'\n",
            pv_status_to_string_func(status));
    return;
  }

  if (keyword_index == -1) {
    // wake-word not found
    return;
  }

  g_message("Detected keyword!\n");

  app->dispatch(ActionType::WAKE, NULL);

  g_debug("Sending prior %d frames\n", g_queue_get_length(frame_buffer));

  while (!g_queue_is_empty(frame_buffer)) {
    AudioFrame *queued_frame = (AudioFrame *)g_queue_pop_head(frame_buffer);
    app->dispatch(ActionType::SPEECH_FRAME, queued_frame);
  }

  transition(State::WOKE);
}

void genie::AudioInput::loop_woke() {
  AudioFrame *new_frame = read_frame(AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (!new_frame) {
    return;
  }

  app->dispatch(ActionType::SPEECH_FRAME, new_frame);

  state_woke_frame_count += 1;

  // We stay "woke" for at minimum `min_woke_frame_count` frames
  if (state_woke_frame_count < min_woke_frame_count) {
    return;
  }

  int silence = WebRtcVad_Process(vad_instance, sample_rate, new_frame->samples,
                                  new_frame->length);

  if (silence == VAD_IS_SILENT) {
    state_vad_frame_count += 1;

    if (state_vad_frame_count >= vad_start_frame_count) {
      // We have not detected speech over the start frame count
      app->dispatch(ActionType::SPEECH_NOT_DETECTED, NULL);
      transition(State::WAITING);
    }
  } else if (silence == VAD_NOT_SILENT) {
    // We detected speech, transition to listenting state
    transition(State::LISTENING);
  }
}

void genie::AudioInput::loop_listening() {
  AudioFrame *new_frame = read_frame(AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (!new_frame) {
    return;
  }

  app->dispatch(ActionType::SPEECH_FRAME, new_frame);

  int silence = WebRtcVad_Process(vad_instance, sample_rate, new_frame->samples,
                                  new_frame->length);

  if (silence == VAD_IS_SILENT) {
    state_vad_frame_count += 1;
  } else if (silence == 1) {
    state_vad_frame_count = 0;
  }
  if (state_vad_frame_count >= vad_done_frame_count) {
    app->dispatch(ActionType::SPEECH_DONE, NULL);
    transition(State::WAITING);
  }
}

void *genie::AudioInput::loop(gpointer data) {
  AudioInput *obj = reinterpret_cast<AudioInput *>(data);

  while (obj->running) {
    switch (obj->state) {
    case State::WAITING:
      obj->loop_waiting();
      break;
    case State::WOKE:
      obj->loop_woke();
      break;
    case State::LISTENING:
      obj->loop_listening();
      break;
    }
  }

  return NULL;
}
