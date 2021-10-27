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

#include <dlfcn.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>

#include "audioinput.hpp"

// Define the following to dump audio streams for debugging reasons
// #define DEBUG_DUMP_STREAMS

#ifdef DEBUG_DUMP_STREAMS
FILE *fp_input;
FILE *fp_input_mono;
FILE *fp_playback;
FILE *fp_filter;
#endif

genie::AudioInput::AudioInput(App *appInstance)
    : app(appInstance), vad_instance(WebRtcVad_Create()), alsa_handle(nullptr),
      pulse_handle(nullptr), echo_state(nullptr), pp_state(nullptr),
      state(State::WAITING) {}

genie::AudioInput::~AudioInput() {
  WebRtcVad_Free(vad_instance);
  free(pcm);
  free(pcm_mono);
  free(pcm_playback);
  free(pcm_filter);
  if (alsa_handle != NULL) {
    snd_pcm_close(alsa_handle);
  }
  if (pulse_handle != NULL) {
    pa_simple_free(pulse_handle);
  }
  if (pp_state) {
    speex_preprocess_state_destroy(pp_state);
    pp_state = NULL;
  }
  if (echo_state) {
    speex_echo_state_destroy(echo_state);
    echo_state = NULL;
  }
#ifdef DEBUG_DUMP_STREAMS
  fclose(fp_input);
  fclose(fp_input_mono);
  fclose(fp_playback);
  fclose(fp_filter);
#endif
  delete wakeword;
}

void genie::AudioInput::close() {
  state.store(State::CLOSED);
  input_thread.join();
}

bool genie::AudioInput::init_alsa(gchar *input_audio_device, int channels) {
  alsa_handle = NULL;
  int error_code =
      snd_pcm_open(&alsa_handle, input_audio_device, SND_PCM_STREAM_CAPTURE, 0);
  if (error_code != 0) {
    g_error("'snd_pcm_open' failed with '%s'\n", snd_strerror(error_code));
    return false;
  }

  snd_pcm_hw_params_t *hardware_params = NULL;
  error_code = snd_pcm_hw_params_malloc(&hardware_params);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_malloc' failed with '%s'\n",
            snd_strerror(error_code));
    return false;
  }

  error_code = snd_pcm_hw_params_any(alsa_handle, hardware_params);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_any' failed with '%s'\n",
            snd_strerror(error_code));
    return false;
  }

  error_code = snd_pcm_hw_params_set_access(alsa_handle, hardware_params,
                                            SND_PCM_ACCESS_RW_INTERLEAVED);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_set_access' failed with '%s'\n",
            snd_strerror(error_code));
    return false;
  }

  error_code = snd_pcm_hw_params_set_format(alsa_handle, hardware_params,
                                            SND_PCM_FORMAT_S16_LE);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_set_format' failed with '%s'\n",
            snd_strerror(error_code));
    return false;
  }

  error_code =
      snd_pcm_hw_params_set_rate(alsa_handle, hardware_params, sample_rate, 0);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_set_rate' failed with '%s'\n",
            snd_strerror(error_code));
    return false;
  }

  error_code =
      snd_pcm_hw_params_set_channels(alsa_handle, hardware_params, channels);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params_set_channels' failed with '%s'\n",
            snd_strerror(error_code));
    return false;
  }

  error_code = snd_pcm_hw_params(alsa_handle, hardware_params);
  if (error_code != 0) {
    g_error("'snd_pcm_hw_params' failed with '%s'\n", snd_strerror(error_code));
    return false;
  }

  snd_pcm_hw_params_free(hardware_params);

  error_code = snd_pcm_prepare(alsa_handle);
  if (error_code != 0) {
    g_error("'snd_pcm_prepare' failed with '%s'\n", snd_strerror(error_code));
    return false;
  }

  return true;
}

bool genie::AudioInput::init_pulse() {
  const pa_sample_spec config{/* format */ PA_SAMPLE_S16LE,
                              /* rate */ uint32_t(sample_rate),
                              /* channels */ 1};

  int error;
  if (!(pulse_handle = pa_simple_new(NULL, "Genie", PA_STREAM_RECORD, NULL,
                                     "record", &config, NULL, NULL, &error))) {
    g_error("pa_simple_new() failed: %s\n", pa_strerror(error));
    return false;
  }

  return true;
}

bool genie::AudioInput::init_speex() {
  spx_int32_t tmp;

  echo_state = speex_echo_state_init_mc(pv_frame_length,
                                        (sample_rate * 300) / 1000, 1, 1);
  speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &(sample_rate));

  pp_state = speex_preprocess_state_init(pv_frame_length, sample_rate);

  // Not supported with the prebuilt speex
  // tmp = true;
  // speex_preprocess_ctl(pp_state, SPEEX_PREPROCESS_SET_AGC, &tmp);

  tmp = true;
  speex_preprocess_ctl(pp_state, SPEEX_PREPROCESS_SET_DENOISE, &tmp);

  tmp = true;
  speex_preprocess_ctl(pp_state, SPEEX_PREPROCESS_SET_DEREVERB, &tmp);

  tmp = -1;
  speex_preprocess_ctl(pp_state, SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &tmp);

  speex_preprocess_ctl(pp_state, SPEEX_PREPROCESS_SET_ECHO_STATE, echo_state);

  g_print("Initialized speex echo-cancellation\n");

  return true;
}

int genie::AudioInput::init() {
  if (!app->config->audio_input_device) {
    g_error("no input audio device");
    return -1;
  }

  wakeword = new WakeWord(app);
  if (!wakeword->init()) {
    return -1;
  }

  sample_rate = wakeword->sample_rate;
  pv_frame_length = wakeword->pv_frame_length;

  channels = 1;
  if (app->config->audio_input_stereo2mono) {
    channels = 2;
    if (app->config->audio_ec_loopback) {
      channels = 3;
    }
  }

  if (strcmp(app->config->audio_backend, "alsa") == 0) {
    if (!init_alsa(app->config->audio_input_device, channels)) {
      return -1;
    }
  } else if (strcmp(app->config->audio_backend, "pulse") == 0) {
    if (!init_pulse()) {
      return -1;
    }
  } else {
    g_error("invalid audio backend: %s", app->config->audio_backend);
    return -1;
  }

  if (app->config->audio_ec_enabled) {
    if (!init_speex()) {
      return -1;
    }
  }

#ifdef DEBUG_DUMP_STREAMS
  fp_input = fopen("/tmp/input.raw", "wb+");
  fp_input_mono = fopen("/tmp/input_mono.raw", "wb+");
  fp_playback = fopen("/tmp/playback.raw", "wb+");
  fp_filter = fopen("/tmp/filter.raw", "wb+");
#endif

  pcm = (int16_t *)malloc(
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, pv_frame_length) * channels *
      sizeof(int16_t));
  if (!pcm) {
    g_error("failed to allocate memory for audio buffer\n");
    return -2;
  }

  pcm_mono = (int16_t *)malloc(
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, pv_frame_length) *
      sizeof(int16_t));
  if (!pcm) {
    g_error("failed to allocate memory for audio buffer\n");
    return -2;
  }

  pcm_playback = (int16_t *)malloc(
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, pv_frame_length) *
      sizeof(int16_t));
  if (!pcm_playback) {
    g_error("failed to allocate memory for audio buffer\n");
    return -2;
  }

  pcm_filter = (int16_t *)malloc(
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, pv_frame_length) *
      sizeof(int16_t));
  if (!pcm_filter) {
    g_error("failed to allocate memory for audio buffer\n");
    return -2;
  }

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
    g_debug("invalid rate %zd or framelength %d", sample_rate, pv_frame_length);
  }

  vad_start_frame_count = ms_to_frames(AUDIO_INPUT_VAD_FRAME_LENGTH,
                                       app->config->vad_start_speaking_ms);
  g_message("Calculated start VAD: %zd ms -> %zd frames",
            app->config->vad_start_speaking_ms, vad_start_frame_count);

  vad_done_frame_count = ms_to_frames(AUDIO_INPUT_VAD_FRAME_LENGTH,
                                      app->config->vad_done_speaking_ms);
  g_message("Calculated done VAD: %zd ms -> %zd frames",
            app->config->vad_done_speaking_ms, vad_done_frame_count);

  vad_input_detected_noise_frame_count = ms_to_frames(
      AUDIO_INPUT_VAD_FRAME_LENGTH, app->config->vad_input_detected_noise_ms);
  g_message("Calculated input detection consecutive noise frame count: %zd ms "
            "-> %zd frames",
            app->config->vad_input_detected_noise_ms,
            vad_input_detected_noise_frame_count);

  g_print("Initialized audio input\n");
  input_thread = std::thread(&AudioInput::loop, this);
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
  int read_frames = 0;
  int error;

  if (alsa_handle != NULL) {
    read_frames = snd_pcm_readi(alsa_handle, pcm, frame_length);
    if (read_frames < 0) {
      g_critical("'snd_pcm_readi' failed with '%s'", snd_strerror(read_frames));
      return AudioFrame(0);
    }
  } else if (pulse_handle != NULL) {
    read_frames = frame_length * sizeof(int16_t);
    if (pa_simple_read(pulse_handle, pcm, read_frames, &error) < 0) {
      g_critical("pa_simple_read() failed with '%s'", pa_strerror(error));
      return AudioFrame(0);
    }
    read_frames /= sizeof(int16_t);
  }

  if (read_frames != frame_length) {
    g_message("read %d frames instead of %d", read_frames, frame_length);
    return AudioFrame(0);
  }

  int16_t *pcm_out = pcm;

  if (alsa_handle != NULL && channels >= 2) {
    // lossy stereo to mono conversion for the first 2 channels (l/r)
    // extract the playback signal from the 3rd channel
    for (int32_t i = 0, j = 0; i < (frame_length * channels);
         i += channels, j++) {
      int16_t left = *(int16_t *)&pcm[i];
      int16_t right = *(int16_t *)&pcm[i + 1];
      if (app->config->audio_input_stereo2mono) {
        int16_t mix = (int16_t)((int32_t(left) + right) / 2);
        pcm_mono[j] = mix;
      } else {
        pcm_mono[j] = left;
      }
      if (app->config->audio_ec_loopback && channels == 3) {
        int16_t ref = *(int16_t *)&pcm[i + 2];
        pcm_playback[j] = ref;
      }
    }

    if (app->config->audio_ec_enabled && channels == 3) {
      speex_echo_cancellation(echo_state, (const spx_int16_t *)pcm_mono,
                              (const int16_t *)pcm_playback,
                              (spx_int16_t *)pcm_filter);

      /* preprecessor is run after AEC. This is not a mistake! */
      if (pp_state) {
        speex_preprocess_run(pp_state, (spx_int16_t *)pcm_filter);
      }

#ifdef DEBUG_DUMP_STREAMS
      fwrite(pcm_playback, sizeof(int16_t), frame_length, fp_playback);
      fwrite(pcm_filter, sizeof(int16_t), frame_length, fp_filter);
#endif
      pcm_out = pcm_filter;
    } else {
      pcm_out = pcm_mono;
    }
#ifdef DEBUG_DUMP_STREAMS
    fwrite(pcm_mono, sizeof(int16_t), frame_length, fp_input_mono);
#endif
  }

#ifdef DEBUG_DUMP_STREAMS
  fwrite(pcm, sizeof(int16_t), frame_length * channels, fp_input);
#endif

  AudioFrame frame(frame_length);
  memcpy(frame.samples, pcm_out, frame_length * sizeof(int16_t));
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
    case State::CLOSED:
      g_critical(
          "Unexpected transition to CLOSED state from inside the input thread");
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
  bool detected = wakeword->process(&new_frame);

  // Add the new frame to the queue
  frame_buffer.push(std::move(new_frame));

  if (!detected) {
    // wake-word not found
    return;
  }

  app->dispatch(new state::events::Wake());

  g_debug("Sending prior %zd frames\n", frame_buffer.size());

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
  int vad_result =
      WebRtcVad_Process(vad_instance, sample_rate, new_frame.samples,
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
    app->dispatch(new state::events::InputDone(false));
    transition(State::WAITING);
  }
}

void genie::AudioInput::loop_listening() {
  AudioFrame new_frame = read_frame(AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (new_frame.length == 0) {
    return;
  }

  app->dispatch(new state::events::InputFrame(std::move(new_frame)));

  int silence = WebRtcVad_Process(vad_instance, sample_rate, new_frame.samples,
                                  AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (silence == VAD_IS_SILENT) {
    state_vad_silent_count += 1;
  } else if (silence == VAD_NOT_SILENT) {
    state_vad_silent_count = 0;
  }
  if (state_vad_silent_count >= vad_done_frame_count) {
    app->dispatch(new state::events::InputDone(true));
    transition(State::WAITING);
  }
}

void genie::AudioInput::loop() {
  for (;;) {
    switch (state) {
      case State::CLOSED:
        return;
      case State::WAITING:
        loop_waiting();
        break;
      case State::WOKE:
        loop_woke();
        break;
      case State::LISTENING:
        loop_listening();
        break;
    }
  }
}
