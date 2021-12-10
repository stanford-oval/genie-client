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

#include "../../app.hpp"
#include "../audiodriver.hpp"

#include <alsa/asoundlib.h>

namespace genie {

class AudioVolumeDriverAlsa : public AudioVolumeDriver {
public:
  AudioVolumeDriverAlsa(App *app) : app(app){};
  virtual ~AudioVolumeDriverAlsa(){};
  virtual void set_volume(int volume);
  virtual int get_volume();
  virtual void duck();
  virtual void unduck();

private:
  App *app;
  bool ducked = false;
  int base_volume = 0;

  snd_mixer_elem_t *get_mixer_element(snd_mixer_t *handle,
                                      const char *selem_name);
};

} // namespace genie
