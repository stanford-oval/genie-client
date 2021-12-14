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

#include "volume.hpp"
#include "../audiovolume.hpp"

static const char *DUCKING_DEVICE = "hw:0";
static const char *DUCKING_CONTROL = "hd";

void genie::AudioVolumeDriverAlsa::duck() {
  if (ducked)
    return;

  set_volume(AudioVolumeController::MIN_VOLUME, DUCKING_DEVICE,
             DUCKING_CONTROL);
  ducked = true;
}

void genie::AudioVolumeDriverAlsa::unduck() {
  if (!ducked)
    return;

  set_volume(AudioVolumeController::MAX_VOLUME, DUCKING_DEVICE,
             DUCKING_CONTROL);
  ducked = false;
}

/**
 * Linearly convert a value between two ranges.
 */
static long convert_to_range(long value, long from_min, long from_max,
                             long to_min, long to_max) {
  return to_min +
         ((value - from_min) * (to_max - to_min) / (from_max - from_min));
}

void genie::AudioVolumeDriverAlsa::set_volume(int volume) {
  set_volume(volume, app->config->audio_output_device,
             app->config->audio_volume_control);
}

void genie::AudioVolumeDriverAlsa::set_volume(int volume,
                                              const char *device_name,
                                              const char *ctl_name) {
  snd_mixer_t *handle = NULL;
  snd_mixer_selem_id_t *sid;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, device_name);
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, ctl_name);
  snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);
  if (!elem) {
    g_warning("Cannot find mixer control element %s in device %s", ctl_name,
              device_name);
    snd_mixer_close(handle);
    return;
  }

  long min, max;
  int err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
  if (err != 0) {
    g_warning("Error getting playback volume range, code=%d", err);
    snd_mixer_close(handle);
    return;
  }

  snd_mixer_selem_set_playback_volume_all(
      elem, convert_to_range(volume, AudioVolumeController::MIN_VOLUME,
                             AudioVolumeController::MAX_VOLUME, min, max));
  snd_mixer_close(handle);
}

int genie::AudioVolumeDriverAlsa::get_volume() {
  return get_volume(app->config->audio_output_device,
                    app->config->audio_volume_control);
}

int genie::AudioVolumeDriverAlsa::get_volume(const char *device_name,
                                             const char *ctl_name) {
  snd_mixer_t *handle = NULL;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_t *sid;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, device_name);
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, ctl_name);
  elem = snd_mixer_find_selem(handle, sid);
  if (!elem) {
    g_warning("Cannot find mixer control element %s in device %s", ctl_name,
              device_name);
    snd_mixer_close(handle);
    return 0;
  }

  long current;
  int err =
      snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &current);
  if (err != 0) {
    g_warning("Error getting current playback volume, code=%d", err);
    snd_mixer_close(handle);
    return 0;
  }

  long min, max;
  err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
  if (err != 0) {
    g_warning("Error getting playback volume range, code=%d", err);
    snd_mixer_close(handle);
    return 0;
  }

  return convert_to_range(current, min, max, AudioVolumeController::MIN_VOLUME,
                          AudioVolumeController::MAX_VOLUME);
}
