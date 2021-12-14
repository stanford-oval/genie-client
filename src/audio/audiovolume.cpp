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

#include "alsa/volume.hpp"
#include "pulseaudio/volume.hpp"

genie::AudioVolumeController::AudioVolumeController(App *app) : app(app) {
  if (app->config->audio_backend == AudioDriverType::ALSA)
    driver = std::make_unique<AudioVolumeDriverAlsa>(app);
  else
    driver = std::make_unique<AudioVolumeDriverPulseAudio>(app);
}

void genie::AudioVolumeController::duck() { driver->duck(); }

void genie::AudioVolumeController::unduck() { driver->unduck(); }

void genie::AudioVolumeController::set_volume(int volume) {
  if (volume > MAX_VOLUME) {
    g_message("Can not adjust playback volume to %d, max is %d. Capping at max",
              volume, MAX_VOLUME);
    volume = MAX_VOLUME;
  } else if (volume < MIN_VOLUME) {
    g_message("Can not adjust playback volume to %d; min is %d. Capping at min",
              volume, MIN_VOLUME);
    volume = MIN_VOLUME;
  }

  driver->set_volume(volume);
  g_message("Updated playback volume to %d", volume);
}

void genie::AudioVolumeController::adjust_volume(int delta) {
  auto current = driver->get_volume();
  auto updated = current + delta;
  set_volume(updated);
}

void genie::AudioVolumeController::increment_volume() {
  adjust_volume(VOLUME_DELTA);
}

void genie::AudioVolumeController::decrement_volume() {
  adjust_volume(-VOLUME_DELTA);
}
