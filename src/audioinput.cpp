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

#define G_LOG_DOMAIN "genie-audioinput"

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
FILE *fp_playback;
FILE *fp_filter;
#endif

// Lifecycle
// ===========================================================================

genie::AudioInput::AudioInput(App *appInstance) {
  app = appInstance;
  vad_instance = WebRtcVad_Create();
  frame_buffer = g_queue_new();
  playback_frames = new AudioFrame *[PLAYBACK_MAX_FRAMES];
  playback_frames_length = 0;
}

genie::AudioInput::~AudioInput() {
  running = false;
  for (size_t i = 0; i < playback_frames_length; i++) {
    delete playback_frames[i];
  }
  delete playback_frames;
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
  fclose(fp_playback);
  fclose(fp_filter);
#endif
}

// Setup
// ===========================================================================

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

  int32_t sample_rate_i = pv_sample_rate_func();

  if (sample_rate_i <= 0) {
    g_error("Picovoice returned a bad frame rate: %d", sample_rate_i);
    return -2;
  }

  sample_rate = (size_t)sample_rate_i;

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

  int32_t pv_frame_length_i = pv_porcupine_frame_length_func();
  if (pv_frame_length_i <= 0) {
    g_error("Picovoice returned a bad frame length: %d", pv_frame_length_i);
    return -2;
  }
  pv_frame_length = (size_t)pv_frame_length_i;

  frame_buffer_size__samples =
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, (int)pv_frame_length);

  pcm = (int16_t *)malloc(frame_buffer_size__samples * BYTES_PER_SAMPLE);
  if (!pcm) {
    g_error("failed to allocate memory for pcm audio buffer\n");
    return -2;
  }

  pcm_playback =
      (int16_t *)malloc(frame_buffer_size__samples * BYTES_PER_SAMPLE);
  if (!pcm_playback) {
    g_error("failed to allocate memory for pcm_playback audio buffer\n");
    return -2;
  }

  pcm_filter = (int16_t *)malloc(frame_buffer_size__samples * BYTES_PER_SAMPLE);
  if (!pcm_filter) {
    g_error("failed to allocate memory for pcm_filter audio buffer\n");
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
  
  // Acoustic Echo Cancelation (AEC)
  // ===========================================================================
  
  if (app->m_config->aec_enabled) {
    g_message("Acoustic Echo Cancellation (AEC) enabled, initializing...");
    
    echo_state = speex_echo_state_init_mc(
        pv_frame_length, ms_to_frames(AUDIO_INPUT_VAD_FRAME_LENGTH, 500), 1, 1);
    speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &(sample_rate));

    PaUtil_AdvanceRingBufferReadIndex(
        &app->m_audio_fifo.get()->ring_buffer,
        PaUtil_GetRingBufferReadAvailable(
            &app->m_audio_fifo.get()->ring_buffer));

#ifdef DEBUG_DUMP_STREAMS
    fp_input = fopen("/tmp/input.raw", "wb+");
    fp_playback = fopen("/tmp/playback.raw", "wb+");
    fp_filter = fopen("/tmp/filter.raw", "wb+");
#endif

  } else {
    g_message("Acoustic Echo Cancellation (AEC) disabled.");
  }
  
  // Thread Spawn
  // =========================================================================
  
  running = true;
  GError *thread_error = NULL;
  g_thread_try_new("audioInputThread", (GThreadFunc)loop, this, &thread_error);
  if (thread_error) {
    g_error("audioInputThread Error: g_thread_try_new() %s",
            thread_error->message);
    g_error_free(thread_error);
    return false;
  }

  return 0;
}

// Utilities
// ===========================================================================

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
int32_t genie::AudioInput::ms_to_frames(size_t frame_length, int32_t ms) {
  //     floor <-  (samples/sec * (     seconds     )) / (samples/frame)
  return (int32_t)((sample_rate * ((double)ms / 1000)) / frame_length);
}

// Audio Frame Creation
// ===========================================================================

/**
 * @brief Read a frame of `frame_length` samples from the `alsa_handle`. Blocks
 * until that many samples are available (or a read error occurs).
 *
 * The caller is responsible for deleting the AudioFrame when done with it.
 *
 * @param frame_length__samples Number of samples to read into the frame.
 *
 * @return The new audio frame, or `NULL` on error.
 */
genie::AudioFrame *genie::AudioInput::read_frame(size_t frame_length__samples) {
  // Block until we read a frame of samples from the input Alsa handle (or an
  // error occurs)
  const snd_pcm_sframes_t read_size__samples =
      snd_pcm_readi(alsa_handle, pcm, frame_length__samples);

  // Our best estimate for an end timestamp of the samples we just read
  std::chrono::time_point<std::chrono::steady_clock> captured_at =
      std::chrono::steady_clock::now();

  // Check for read errors

  if (read_size__samples < 0) {
    g_critical("'snd_pcm_readi' failed with '%s'\n",
               snd_strerror(read_size__samples));
    return NULL;
  }

  if (read_size__samples != (int)frame_length__samples) {
    g_message("read %d frames instead of %d\n", read_size__samples,
              frame_length__samples);
    return NULL;
  }

  AudioFrame *input_frame = new AudioFrame(frame_length__samples, captured_at);

  // If AEC is enabled hand off to that method to finish up
  if (app->m_config->aec_enabled) {
    return aec_process(input_frame);
  }

  // Otherwise simply copy `pcm` into the frame and return it
  memcpy(input_frame->samples, pcm, frame_length__samples * BYTES_PER_SAMPLE);
  return input_frame;
}

// Acoustic Echo Cancelation (AEC)
// ---------------------------------------------------------------------------

/**
 * @brief Apply Acoustic Echo Cancelation (AEC) to the read frame sitting in
 * the `pcm` buffer and copy the results to `input_frame`, completing it's
 * creation.
 *
 * @param input_frame The newly-created frame, with no `samples`.
 *
 * @return The completed frame, or `NULL` on failure.
 */
genie::AudioFrame *genie::AudioInput::aec_process(AudioFrame *input_frame) {
  // See how many AudioFrame (of variable, non-zero length) are available in the
  // playback ring
  ring_buffer_size_t playback_available__frames =
      PaUtil_GetRingBufferReadAvailable(&app->m_audio_fifo.get()->ring_buffer);

  // Annoying, seemingly, `ring_buffer_size_t` is signed. No clue what to do
  // with it if it's negative...
  if (playback_available__frames < 0) {
    g_error("Negative amount of frames (%d) available in ring buffer\n",
            playback_available__frames);
    return NULL;
  }

  if (playback_available__frames > (int)PLAYBACK_MAX_FRAMES) {
    g_error(
        "Too many frame objects in the playback ring! Found %d, max is %d\n",
        playback_available__frames, PLAYBACK_MAX_FRAMES);
    return NULL;
  }

  // Read ALL the frames in the ring buffer -- get 'em outta there!
  playback_frames_length =
      PaUtil_ReadRingBuffer(&app->m_audio_fifo.get()->ring_buffer,
                            playback_frames, playback_available__frames);

  fill_pcm_playback(input_frame);

  speex_echo_cancellation(echo_state, (const spx_int16_t *)pcm,
                          (const spx_int16_t *)pcm_playback,
                          (spx_int16_t *)pcm_filter);

  memcpy(input_frame->samples, pcm_filter,
         input_frame->length * BYTES_PER_SAMPLE);

#ifdef DEBUG_DUMP_STREAMS
  fwrite(pcm, BYTES_PER_SAMPLE, input_frame->length, fp_input);
  fwrite(pcm_playback, BYTES_PER_SAMPLE, input_frame->length, fp_playback);
  fwrite(pcm_filter, BYTES_PER_SAMPLE, input_frame->length, fp_filter);
#endif

  return input_frame;
}

/**
 * @brief Fill `pcm_playback` buffer from playback audio frames captured by
 * the `genie::AudioFIFO` instance.
 *
 * @param input_frame Pointer to the audio frame being created. Importantly
 * includes the frame length (in _samples_) and capture timestamp.
 */
void genie::AudioInput::fill_pcm_playback(genie::AudioFrame *input_frame) {
  // Variable to track what time-stamp we've filled the PCM playback buffer to.
  //
  // Since we haven't filled it at all yet this will be the end of the input
  // frame, which we estimate by when it was captured by us.
  auto filled_to_timestamp = input_frame->captured_at;

  int samples_to_fill = input_frame->length;

  for (int i = playback_frames_length - 1; i >= 0; i--) {
    AudioFrame *pb_frame = playback_frames[i];
    g_message("Recon from frame of %d samples, index %d\n", pb_frame->length,
              i);

    // We only need to dive into the playback frame if we have samples left to
    // fill
    if (samples_to_fill > 0) {
      // We have `samples` number of samples left to fill in `pcm_playback`

      // Zero-Fill
      // =====================================================================
      //
      // Fill empty samples into the PCM playback buffer for the period:
      //
      // -    FROM the end of the playback frame (which we estimate by when it
      //      was captured by us)
      // -    TO the timestamp we have already back-filled to
      //
      int gap_duration__ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              filled_to_timestamp - pb_frame->captured_at)
              .count();
      if (gap_duration__ns > 0) {
        int gap_samples = gap_duration__ns / SAMPLE_DURATION.count();
        if (gap_samples > 0) {
          if (gap_samples > samples_to_fill) {
            // The gap is longer than the amount of remaining samples we need to
            // to fill, so cap it at the amount we need
            gap_samples = samples_to_fill;
          }
          memset(
              // Examples:
              //
              //   start   +   samples to fill   - gap size = write index
              //     0     + 1                   - 1        = 0 (first sample)
              //     0     + 512                 - 1        = 511 (last sample)
              //     0     + 10                  - 2        = 8 ([8, 9])
              //     0     + 10                  - 4        = 6 ([6, 7, 8, 9])
              pcm_playback + samples_to_fill - gap_samples, 0,
              gap_samples * BYTES_PER_SAMPLE);
          samples_to_fill -= gap_samples;
        }
      }

      // Playback Frame Copy
      // =====================================================================

      int offset = 0;
      int samples_to_copy = pb_frame->length;
      if ((int)pb_frame->length > samples_to_fill) {
        // We only need the last part of the frame
        //
        // Example: Say the frame is length=2 and we want samples=1, then we
        // need to offset by the difference: length - samples = 2 - 1 = 1
        offset = pb_frame->length - samples_to_fill;
        samples_to_copy = samples_to_fill;
      }
      // Copy `size` samples from playback frame to PCM playback buffer
      memcpy(pcm_playback + samples_to_fill - samples_to_copy,
             pb_frame->samples + offset, samples_to_copy * BYTES_PER_SAMPLE);

      g_message("Coppied %d samples of %d from playback frame %ld\n",
                samples_to_copy, pb_frame->length,
                pb_frame->captured_at.time_since_epoch().count());
      // Deduct `size` samples that we just coppied from the `samples` number
      // we still need
      samples_to_fill -= samples_to_copy;
      // Set the reference time to the "start" of the playback frame we just
      // consumed
      filled_to_timestamp = pb_frame->started_at();
    } // if (samples > 0)

    // Deallocate Playback Frame
    // =======================================================================
    //
    // We're done with it.
    //
    delete pb_frame;
  }

  // We're through with all the playback frames, see if we still need to fill
  // more samples into the PCM playback buffer
  if (samples_to_fill > 0) {
    // We didn't find enough playback samples; zero fill the rest
    memset(pcm_playback, 0, samples_to_fill * BYTES_PER_SAMPLE);
  }
} // fill_pcm_playback()

// State
// ===========================================================================

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

// Looping
// ===========================================================================

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
