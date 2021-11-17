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

#include "alsa.hpp"

// Define the following to dump audio streams for debugging reasons
// #define DEBUG_DUMP_STREAMS

#ifdef DEBUG_DUMP_STREAMS
FILE *fp_input;
FILE *fp_input_mono;
FILE *fp_playback;
FILE *fp_filter;
#endif

genie::AudioInputAlsa::AudioInputAlsa(App *app) : app(app) {}

genie::AudioInputAlsa::~AudioInputAlsa() {
  free(pcm);
  free(pcm_mono);
  free(pcm_playback);
  free(pcm_filter);
  if (alsa_handle != NULL) {
    snd_pcm_close(alsa_handle);
  }
#ifdef DEBUG_DUMP_STREAMS
  fclose(fp_input);
  fclose(fp_input_mono);
  fclose(fp_playback);
  fclose(fp_filter);
#endif
}

bool genie::AudioInputAlsa::init_pcm(gchar *input_audio_device) {
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

bool genie::AudioInputAlsa::init_speex() {
  spx_int32_t tmp;

  echo_state = speex_echo_state_init_mc(
      frame_length,
      (sample_rate * 300) / 1000, 1, 1);
  speex_echo_ctl(echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &(sample_rate));

  pp_state = speex_preprocess_state_init(512, sample_rate);

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

bool genie::AudioInputAlsa::init(gchar *audio_input_device, int m_sample_rate,
                                 int m_channels, int max_frame_length) {
  if (!audio_input_device) {
    g_error("no input audio device");
    return false;
  }
  sample_rate = m_sample_rate;
  frame_length = max_frame_length;

  channels = 1;
  if (app->config->audio_input_stereo2mono) {
    channels = 2;
    if (app->config->audio_ec_loopback) {
      channels = 3;
    }
  }

  if (!init_pcm(audio_input_device)) {
    return false;
  }

  if (app->config->audio_ec_enabled) {
    if (!init_speex()) {
      return false;
    }
  }

#ifdef DEBUG_DUMP_STREAMS
  fp_input = fopen("/tmp/input.raw", "wb+");
  fp_input_mono = fopen("/tmp/input_mono.raw", "wb+");
  fp_playback = fopen("/tmp/playback.raw", "wb+");
  fp_filter = fopen("/tmp/filter.raw", "wb+");
#endif
  pcm = (int16_t *)malloc(max_frame_length * channels * sizeof(int16_t));
  if (!pcm) {
    g_error("failed to allocate memory for audio buffer\n");
    return false;
  }

  pcm_mono = (int16_t *)malloc(max_frame_length * sizeof(int16_t));
  if (!pcm) {
    g_error("failed to allocate memory for audio buffer\n");
    return false;
  }

  pcm_playback = (int16_t *)malloc(max_frame_length * sizeof(int16_t));
  if (!pcm_playback) {
    g_error("failed to allocate memory for audio buffer\n");
    return false;
  }

  pcm_filter = (int16_t *)malloc(max_frame_length * sizeof(int16_t));
  if (!pcm_filter) {
    g_error("failed to allocate memory for audio buffer\n");
    return false;
  }

  return true;
}

genie::AudioFrame genie::AudioInputAlsa::read_frame(int32_t frame_length) {
  int read_frames = 0;
  int error;

  if (alsa_handle != NULL) {
    read_frames = snd_pcm_readi(alsa_handle, pcm, frame_length);
    if (read_frames < 0) {
      g_critical("'snd_pcm_readi' failed with '%s'", snd_strerror(read_frames));
      return AudioFrame(0);
    }
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
