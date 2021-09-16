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

#include "audiofifo.hpp"
#include "audioinput.hpp"
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
  state = State::WAITING;
}

genie::AudioInput::~AudioInput() {
  running = false;
  WebRtcVad_Free(vad_instance);
  free(pcm);
  snd_pcm_close(alsa_handle);
  pv_porcupine_delete_func(porcupine);
  dlclose(porcupine_library);
}

int genie::AudioInput::init() {
  gchar *input_audio_device = app->config->audioInputDevice;

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

  int32_t sample_rate_signed = pv_sample_rate_func();

  g_assert(sample_rate_signed > 0);
  sample_rate = (size_t)sample_rate_signed;

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
                                       app->config->vad_start_speaking_ms);
  g_message("Calculated start VAD: %d ms -> %d frames",
            app->config->vad_start_speaking_ms, vad_start_frame_count);

  vad_done_frame_count = ms_to_frames(AUDIO_INPUT_VAD_FRAME_LENGTH,
                                      app->config->vad_done_speaking_ms);
  g_message("Calculated done VAD: %d ms -> %d frames",
            app->config->vad_done_speaking_ms, vad_done_frame_count);

  vad_input_detected_noise_frame_count = ms_to_frames(
      AUDIO_INPUT_VAD_FRAME_LENGTH, app->config->vad_input_detected_noise_ms);
  g_message("Calculated input detection consecutive noise frame count: %d ms "
            "-> %d frames",
            app->config->vad_input_detected_noise_ms,
            vad_input_detected_noise_frame_count);

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
 * @brief Tell the audio input loop (running on it's own thread) to start
 * listening if it wasn't.
 *
 * Essentially synthesizes hearing the wake-word, hence the name.
 *
 * This method is designed to be _thread-safe_ -- while the audio input loop
 * runs on it's own thread, this method can be called from any thread, allowing
 * listening to be triggered externally.
 *
 * The method works by changing `this->state` to `State::WAKE` if it was
 * `State::WAITING`, which is picked up by the next loop iteration in the
 * audio input thread.
 */
void genie::AudioInput::wake() {
  State expect = State::WAITING;
  // SEE  https://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange
  //
  // Ok, the way I understand this is:
  //
  // If the value of `this->state` is `expect` -- which is `State::WAITING` --
  // then change it to `State::WOKE`.
  //
  // If the value of `this->state` is _not_ `expect` (`State::WAITING`) then
  // write the value of `this->state` over `expect`.
  //
  // Since we only want to change the value of `this->state` _if_ it is
  // `State::WAITING` and don't want to do anything if `this->state` is anything
  // else, this should be all we need -- we don't care that `expect` gets set
  // to something else when `this->state` was not `State::WAITING`.
  //
  state.compare_exchange_strong(expect, State::WOKE);
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
size_t genie::AudioInput::ms_to_frames(size_t frame_length, size_t ms) {
  //     floor <-  (samples/sec * (     seconds     )) / (samples/frame)
  g_assert(sample_rate > 0);
  return (size_t)((sample_rate * ((double)ms / 1000)) / frame_length);
}

genie::AudioFrame genie::AudioInput::read_frame(int32_t frame_length) {
  const int read_frames = snd_pcm_readi(alsa_handle, pcm, frame_length);

  if (read_frames < 0) {
    g_critical("'snd_pcm_readi' failed with '%s'", snd_strerror(read_frames));
    return AudioFrame(0);
  }

  if (read_frames != frame_length) {
    g_message("read %d frames instead of %d", read_frames, frame_length);
    return AudioFrame(0);
  }

  AudioFrame frame(frame_length);
  memcpy(frame.samples, pcm, frame_length * sizeof(int16_t));
  return frame;
}

void genie::AudioInput::transition(State to_state) {
  // Reset state variables
  state_woke_frame_count = 0;
  state_vad_silent_count = 0;
  state_vad_noise_count = 0;

  switch (to_state) {
    case State::WAITING:
      g_message("[AudioInput] -> State::WAITING");
      state = State::WAITING;
      break;
    case State::WOKE:
      g_message("[AudioInput] -> State::WOKE");
      state = State::WOKE;
      break;
    case State::LISTENING:
      g_message("[AudioInput] -> State::LISTENING");
      state = State::LISTENING;
      break;
  }
}

void genie::AudioInput::loop_waiting() {
  AudioFrame new_frame = read_frame(pv_frame_length);

  if (new_frame.length == 0) {
    return;
  }

  // Drop from the queue if there are too many items
  while (frame_buffer.size() > BUFFER_MAX_FRAMES) {
    frame_buffer.pop();
  }

  // Check the new frame for the wake-word
  int32_t keyword_index = -1;
  pv_status_t status =
      pv_porcupine_process_func(porcupine, pcm, &keyword_index);

  if (status != PV_STATUS_SUCCESS) {
    // Picovoice error!
    g_critical("'pv_porcupine_process' failed with '%s'\n",
               pv_status_to_string_func(status));
    return;
  }

  // Add the new frame to the queue
  frame_buffer.push(std::move(new_frame));

  if (keyword_index == -1) {
    // wake-word not found
    return;
  }

  g_message("Detected keyword!\n");

  app->dispatch(new state::events::Wake());

  g_debug("Sending prior %d frames\n", frame_buffer.size());

  while (!frame_buffer.empty()) {
    app->dispatch(
        new state::events::InputFrame(std::move(frame_buffer.front())));
    frame_buffer.pop();
  }

  transition(State::WOKE);
}

void genie::AudioInput::loop_woke() {
  AudioFrame new_frame = read_frame(AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (new_frame.length == 0) {
    return;
  }

  app->dispatch(new state::events::InputFrame(std::move(new_frame)));

  state_woke_frame_count += 1;

  // Run Voice Activity Detection (VAD) against the frame
  int vad_result = WebRtcVad_Process(vad_instance, sample_rate, pcm,
                                     AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (vad_result == VAD_IS_SILENT) {
    // We picked up silence
    //
    // Increment the silent count
    state_vad_silent_count += 1;
    // Set the noise count to zero
    state_vad_noise_count = 0;
  } else if (vad_result == VAD_NOT_SILENT) {
    // We picked up silence
    //
    // Increment the noise count
    state_vad_noise_count += 1;
    // Set the silent count to zero
    state_vad_silent_count = 0;

    if (state_vad_noise_count >= vad_input_detected_noise_frame_count) {
      // We have measured the configured amount of consecutive noise frames,
      // which means we have detected input.
      //
      // Transition to `State::LISTENING`
      transition(State::LISTENING);
      return;
    }
  }

  if (state_woke_frame_count >= vad_start_frame_count) {
    // We have not detected speech over the start frame count, give up
    app->dispatch(new state::events::InputNotDetected());
    transition(State::WAITING);
  }
}

void genie::AudioInput::loop_listening() {
  AudioFrame new_frame = read_frame(AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (new_frame.length == 0) {
    return;
  }

  app->dispatch(new state::events::InputFrame(std::move(new_frame)));

  int silence = WebRtcVad_Process(vad_instance, sample_rate, pcm,
                                  AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (silence == VAD_IS_SILENT) {
    state_vad_silent_count += 1;
  } else if (silence == VAD_NOT_SILENT) {
    state_vad_silent_count = 0;
  }
  if (state_vad_silent_count >= vad_done_frame_count) {
    app->dispatch(new state::events::InputDone());
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
