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

genie::AudioVolumeController::AudioVolumeController(App *appInstance)
    : app(appInstance), ducked(false) {}

genie::AudioVolumeController::~AudioVolumeController() {
  if (pulse_handle != NULL) {
    pa_simple_free(pulse_handle);
  }
}

int genie::AudioVolumeController::duck() {
  if (ducked) return true;

  if (strcmp(app->config->audio_backend, "alsa") == 0) {
    system("amixer -D hw:audiocodec cset name='hd' 0");
  } else if (strcmp(app->config->audio_backend, "pulse") == 0) {
    const pa_sample_spec config{/* format */ PA_SAMPLE_S16LE,
                                /* rate */ 44100,
                                /* channels */ 2};
    pulse_handle = pa_simple_new(NULL,
                  "Genie-duck",
                  PA_STREAM_PLAYBACK,
                  NULL,
                  "Dummy Output for Ducking",
                  &config,
                  NULL,
                  NULL,
                  NULL
                  );
  }
  ducked = true;
  return true;
}

int genie::AudioVolumeController::unduck() {
  if (!ducked) return true;

  if (strcmp(app->config->audio_backend, "alsa") == 0) {
    system("amixer -D hw:audiocodec cset name='hd' 255");
  } else if (strcmp(app->config->audio_backend, "pulse") == 0) {
    if (pulse_handle != NULL) {
      pa_simple_free(pulse_handle);
    }
  }
  ducked = false;
  return true;
}
