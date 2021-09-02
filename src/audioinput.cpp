// -*- mode: cpp; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "pv_porcupine.h"

genie::AudioInput::AudioInput(App *appInstance) {
  app = appInstance;
  vadInstance = WebRtcVad_Create();
  pcmQueue = g_queue_new();
}

genie::AudioInput::~AudioInput() {
  running = false;
  WebRtcVad_Free(vadInstance);
  while (!g_queue_is_empty(pcmQueue)) {
    g_free(g_queue_pop_head(pcmQueue));
  }
  g_queue_free(pcmQueue);
  free(pcm);
  snd_pcm_close(alsa_handle);
  pv_porcupine_delete_func(porcupine);
  dlclose(porcupine_library);
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

  frame_length = pv_porcupine_frame_length_func();

  pcm = (int16_t *)malloc(frame_length * sizeof(int16_t));
  if (!pcm) {
    g_error("failed to allocate memory for audio buffer\n");
    return -2;
  }

  g_print("Initialized wakeword engine, frame length %d, sample rate %d\n",
          frame_length, sample_rate);

  if (WebRtcVad_Init(vadInstance)) {
    g_error("failed to initialize webrtc vad\n");
    return -2;
  }

  int vadMode = 3;
  if (WebRtcVad_set_mode(vadInstance, vadMode)) {
    g_error("unable to set vad mode to %d", vadMode);
    return -2;
  }
  vadThreshold = 32;

  if (WebRtcVad_ValidRateAndFrameLength(sample_rate, frame_length)) {
    g_debug("invalid rate %d or framelength %d", sample_rate, frame_length);
  }

  GError *gerror = NULL;
  g_thread_try_new("audioInputThread", (GThreadFunc)loop,
                   reinterpret_cast<AudioInput *>(this), &gerror);
  if (gerror) {
    g_print("audioInputThread Error: g_thread_try_new() %s\n", gerror->message);
    g_error_free(gerror);
    return false;
  }
  running = true;

  return 0;
}

genie::AudioFrame *genie::AudioInput::build_frame(int16_t *samples, gsize length) {
  AudioFrame *frame = g_new(AudioFrame, 1);
  frame->samples = (int16_t *)g_malloc(length * sizeof(int16_t));
  memcpy(frame->samples, samples, length * sizeof(int16_t));
  frame->length = length;
  return frame;
}

void *genie::AudioInput::loop(gpointer data) {
  AudioInput *obj = reinterpret_cast<AudioInput *>(data);
  pv_status_t status;
  int state = INPUT_STATE_WAITING, vadCounter = 0;

  int32_t frame_length = obj->frame_length;

  while (obj->running) {
    const int count = snd_pcm_readi(obj->alsa_handle, obj->pcm, frame_length);
    if (count < 0) {
      g_error("'snd_pcm_readi' failed with '%s'\n", snd_strerror(count));
      break;
    } else if (count != frame_length) {
      g_print("read %d frames instead of %d\n", count, frame_length);
      continue;
    }

    AudioFrame *new_frame = build_frame(obj->pcm, frame_length);

    if (state == INPUT_STATE_WAITING) {
      if (g_queue_get_length(obj->pcmQueue) > AUDIO_INPUT_BUFFER_MAX_FRAMES) {
        AudioFrame *dropped_frame =
            (AudioFrame *)g_queue_pop_head(obj->pcmQueue);
        g_free(dropped_frame->samples);
        g_free(dropped_frame);
      }

      g_queue_push_tail(obj->pcmQueue, new_frame);

      int32_t keyword_index = -1;
      status = obj->pv_porcupine_process_func(
          obj->porcupine, new_frame->samples, &keyword_index);

      if (status != PV_STATUS_SUCCESS) {
        g_error("'pv_porcupine_process' failed with '%s'\n",
                obj->pv_status_to_string_func(status));
        break;
      }

      if (keyword_index != -1) {
        g_message("detected keyword\n");

        obj->app->dispatch(WAKE, NULL);

        g_debug("send prior %d frames\n", g_queue_get_length(obj->pcmQueue));

        while (!g_queue_is_empty(obj->pcmQueue)) {
          AudioFrame *queued_frame =
              (AudioFrame *)g_queue_pop_head(obj->pcmQueue);
          obj->app->dispatch(INPUT_SPEECH_FRAME, queued_frame);
        }

        frame_length = AUDIO_INPUT_VAD_FRAME_LENGTH;
        state = INPUT_STATE_STREAMING;
      }

    } else if (state == INPUT_STATE_STREAMING) {
      obj->app->dispatch(INPUT_SPEECH_FRAME, new_frame);
      int silence = WebRtcVad_Process(obj->vadInstance, obj->sample_rate,
                                      new_frame->samples, new_frame->length);
      if (silence == 0) {
        vadCounter += 1;
      } else if (silence == 1) {
        vadCounter = 0;
      }
      if (vadCounter >= obj->vadThreshold) {
        state = INPUT_STATE_WAITING;
        obj->app->dispatch(INPUT_SPEECH_DONE, NULL);
        vadCounter = 0;
        frame_length = obj->frame_length;
      }
    }
  }

  return NULL;
}
