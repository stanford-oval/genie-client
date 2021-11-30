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

#include "audiovolume.hpp"

genie::AudioVolumeController::AudioVolumeController(App *app)
    : app(app), ducked(false), pulseaudio_client(NULL) {
  if (strcmp(app->config->audio_backend, "pulse") == 0) {
    pulseaudio_client = new PulseAudioClient(app);
    pulseaudio_client->init();
  }
}

genie::AudioVolumeController::~AudioVolumeController() {
  if (pulse_handle != nullptr) {
    pa_simple_free(pulse_handle);
  }
  if (pulseaudio_client) {
    delete pulseaudio_client;
  }
}

int genie::AudioVolumeController::duck() {
  if (ducked)
    return true;

  if (strcmp(app->config->audio_backend, "alsa") == 0) {
    system("amixer -D hw:audiocodec cset name='hd' 0");
  } else if (strcmp(app->config->audio_backend, "pulse") == 0) {
    const pa_sample_spec config{/* format */ PA_SAMPLE_S16LE,
                                /* rate */ 44100,
                                /* channels */ 2};
    pulse_handle =
        pa_simple_new(NULL, "Genie-duck", PA_STREAM_PLAYBACK, NULL,
                      "Dummy Output for Ducking", &config, NULL, NULL, NULL);
  }
  ducked = true;
  return true;
}

int genie::AudioVolumeController::unduck() {
  if (!ducked)
    return true;

  if (strcmp(app->config->audio_backend, "alsa") == 0) {
    system("amixer -D hw:audiocodec cset name='hd' 255");
  } else if (strcmp(app->config->audio_backend, "pulse") == 0) {
    if (pulse_handle != nullptr) {
      pa_simple_free(pulse_handle);
      pulse_handle = nullptr;
    }
  }
  ducked = false;
  return true;
}

snd_mixer_elem_t *
genie::AudioVolumeController::get_mixer_element(snd_mixer_t *handle,
                                                const char *selem_name) {
  snd_mixer_selem_id_t *sid;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, app->config->audio_output_device);
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, selem_name);
  return snd_mixer_find_selem(handle, sid);
}

void genie::AudioVolumeController::set_volume(long volume) {
  if (strcmp(app->config->audio_backend, "alsa") == 0) {
    snd_mixer_t *handle = NULL;
    snd_mixer_selem_id_t *sid;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, app->config->audio_output_device);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "LINEOUT volume");
    snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);

    snd_mixer_selem_set_playback_volume_all(elem, volume);
    snd_mixer_close(handle);
  } else if (strcmp(app->config->audio_backend, "pulse") == 0) {
    pulseaudio_client->set_default_volume(volume);
  }
  g_message("Updated playback volume to %ld", volume);
}

int genie::AudioVolumeController::adjust_playback_volume(long delta) {
  long min, max, current, updated;
  int err = 0;
  if (strcmp(app->config->audio_backend, "alsa") == 0) {
    snd_mixer_t *handle = NULL;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *sid;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, app->config->audio_output_device);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "LINEOUT volume");
    elem = snd_mixer_find_selem(handle, sid);

    err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    if (err != 0) {
      g_warning("Error getting playback volume range, code=%d", err);
      snd_mixer_close(handle);
      return err;
    }

    err = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO,
                                              &current);
    if (err != 0) {
      g_warning("Error getting current playback volume, code=%d", err);
      snd_mixer_close(handle);
      return err;
    }
    snd_mixer_close(handle);
  } else if (strcmp(app->config->audio_backend, "pulse") == 0) {
    min = 0;
    max = 100;
    current = pulseaudio_client->get_default_volume();
  }
  delta *= 5;

  updated = current + delta;

  if (updated > max) {
    g_message(
        "Can not adjust playback volume to %ld, max is %ld. Capping at max",
        updated, max);
    updated = max;
  } else if (updated < min) {
    g_message(
        "Can not adjust playback volume to %ld; min is %ld. Capping at min",
        updated, min);
    updated = min;
  }

  if (updated == current) {
    g_message("Volume already at %ld", current);
    return 0;
  }

  set_volume(updated);
  return 0;
}

int genie::AudioVolumeController::increment_playback_volume() {
  return adjust_playback_volume(1);
}

int genie::AudioVolumeController::decrement_playback_volume() {
  return adjust_playback_volume(-1);
}
