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
#include "pulseaudio.hpp"

#include <alsa/asoundlib.h>
#include <pulse/simple.h>
#include <pulse/error.h>

namespace genie {

class AudioVolumeController {
public:
  AudioVolumeController(App *appInstance);
  ~AudioVolumeController();
  int init();
  int duck();
  int unduck();
  long get_volume();
  void set_volume(long volume);
  int adjust_playback_volume(long delta);
  int increment_playback_volume();
  int decrement_playback_volume();

private:
  App *app;
  bool ducked;

  pa_simple *pulse_handle = NULL;
  PulseAudioClient *pulseaudio_client;

  snd_mixer_elem_t *get_mixer_element(snd_mixer_t *handle,
                                      const char *selem_name);
};

} // namespace genie
