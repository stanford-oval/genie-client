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

#include "audioinput.hpp"
#include "alsa/input.hpp"
#include "pulseaudio/input.hpp"

// note: we need to redefine G_LOG_DOMAIN here or the definition will
// bleed into the functions declared in the header, which will break
// the logging, but also break the One Definition Rule and cause potential havoc
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::AudioInput"

genie::AudioInput::AudioInput(App *app)
    : app(app), vad_instance(WebRtcVad_Create()), wakeword(nullptr),
      input(nullptr), state(State::WAITING) {
  wakeword = std::make_unique<WakeWord>(app);

  sample_rate = wakeword->sample_rate;
  pv_frame_length = wakeword->pv_frame_length;
  int32_t max_frame_length =
      std::max(AUDIO_INPUT_VAD_FRAME_LENGTH, pv_frame_length);
  channels = 1;

  if (app->config->audio_backend == AudioDriverType::ALSA) {
    input = std::make_unique<AudioInputAlsa>(app);
  } else if (app->config->audio_backend == AudioDriverType::PULSEAUDIO) {
    input = std::make_unique<AudioInputPulseSimple>(app);
  } else {
    g_assert_not_reached();
  }

  if (!input->init(app->config->audio_input_device, wakeword->sample_rate,
                   channels, max_frame_length)) {
    g_error("failed to initialized audio input driver");
    return;
  }

  if (WebRtcVad_Init(vad_instance)) {
    g_error("failed to initialize webrtc vad\n");
    return;
  }

  int vadMode = 3;
  if (WebRtcVad_set_mode(vad_instance, vadMode)) {
    g_error("unable to set vad mode to %d", vadMode);
    return;
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

  vad_listen_timeout_frame_count = ms_to_frames(
      AUDIO_INPUT_VAD_FRAME_LENGTH, app->config->vad_listen_timeout_ms);
  g_message("Calculated listen timeout frame count: %zd ms "
            "-> %zd frames",
            app->config->vad_listen_timeout_ms, vad_listen_timeout_frame_count);

  g_message("Initialized audio input with %s backend\n", audio_driver_type_to_string(app->config->audio_backend));
  input_thread = std::thread(&AudioInput::loop, this);
}

genie::AudioInput::~AudioInput() { WebRtcVad_Free(vad_instance); }

void genie::AudioInput::close() {
  state.store(State::CLOSED);
  input_thread.join();
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
  AudioFrame new_frame = input->read_frame(pv_frame_length);

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

  g_message("Wakeword detected in waiting state");
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
  AudioFrame new_frame = input->read_frame(AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (new_frame.length == 0) {
    return;
  }

  state_woke_frame_count += 1;

  // Run Voice Activity Detection (VAD) against the frame

  // NOTE: this must run BEFORE we send the frame to the main thread
  // because the frame will become null when we send it
  int vad_result =
      WebRtcVad_Process(vad_instance, sample_rate, new_frame.samples,
                        AUDIO_INPUT_VAD_FRAME_LENGTH);

  app->dispatch(new state::events::InputFrame(std::move(new_frame)));

  if (vad_result == VAD_IS_SILENT) {
    g_debug("Frame %zu is silent in woke state (silent: %zu, noise: %zu)",
            state_woke_frame_count, state_vad_silent_count,
            state_vad_noise_count);
    // We picked up silence
    //
    // Increment the silent count
    state_vad_silent_count += 1;
    // Set the noise count to zero
    // state_vad_noise_count = 0;
  } else if (vad_result == VAD_NOT_SILENT) {
    g_debug("Frame %zu is not silent in woke state (silent: %zu, noise: %zu)",
            state_woke_frame_count, state_vad_silent_count,
            state_vad_noise_count);
    // We picked up silence
    //
    // Increment the noise count
    state_vad_noise_count += 1;
    // Set the silent count to zero
    state_vad_silent_count = 0;

    if (state_vad_noise_count >= vad_input_detected_noise_frame_count) {
      g_debug("Detected %zu frames of noise, transition to listening",
              state_vad_noise_count);
      // We have measured the configured amount of consecutive noise frames,
      // which means we have detected input.
      //
      // Transition to `State::LISTENING`
      transition(State::LISTENING);
      return;
    }
  }

  if (state_woke_frame_count >= vad_start_frame_count) {
    g_debug("Not detected VAD input after %zu frames", vad_start_frame_count);
    // We have not detected speech over the start frame count, give up
    app->dispatch(new state::events::InputDone(false));
    transition(State::WAITING);
  }
}

void genie::AudioInput::loop_listening() {
  AudioFrame new_frame = input->read_frame(AUDIO_INPUT_VAD_FRAME_LENGTH);

  if (new_frame.length == 0) {
    return;
  }

  state_woke_frame_count += 1;

  // Run Voice Activity Detection (VAD) against the frame

  // NOTE: this must run BEFORE we send the frame to the main thread
  // because the frame will become null when we send it
  int silence = WebRtcVad_Process(vad_instance, sample_rate, new_frame.samples,
                                  AUDIO_INPUT_VAD_FRAME_LENGTH);

  app->dispatch(new state::events::InputFrame(std::move(new_frame)));

  if (silence == VAD_IS_SILENT) {
    g_debug("Frame %zu is silent in listening state (silent: %zu, noise: %zu)",
            state_woke_frame_count, state_vad_silent_count,
            state_vad_noise_count);
    state_vad_silent_count += 1;
  } else if (silence == VAD_NOT_SILENT) {
    g_debug(
        "Frame %zu is not silent in listening state (silent: %zu, noise: %zu)",
        state_woke_frame_count, state_vad_silent_count, state_vad_noise_count);
    state_vad_silent_count = 0;
  }
  if (state_vad_silent_count >= vad_done_frame_count) {
    g_debug("Detected %zu frames of silence, VAD done", state_vad_silent_count);
    app->dispatch(new state::events::InputDone(true));
    transition(State::WAITING);
  } else if (state_woke_frame_count >= vad_listen_timeout_frame_count) {
    g_message("LISTENING timed out after %zu frames (~%zu ms)",
              vad_listen_timeout_frame_count,
              app->config->vad_listen_timeout_ms);
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
