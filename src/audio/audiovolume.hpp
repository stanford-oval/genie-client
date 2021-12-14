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

#include "../app.hpp"
#include "audiodriver.hpp"

#include <pulse/error.h>
#include <pulse/simple.h>

namespace genie {

class AudioVolumeController {
public:
  AudioVolumeController(App *appInstance);
  void init();
  void duck();
  void unduck();
  int get_volume();
  void set_volume(int volume);
  void adjust_volume(int delta);
  void increment_volume();
  void decrement_volume();

  static const int MAX_VOLUME = 100;
  static const int MIN_VOLUME = 0;
  static const int VOLUME_DELTA = 10;

private:
  App *app;
  bool ducked;
  std::unique_ptr<AudioVolumeDriver> driver;
};

} // namespace genie
