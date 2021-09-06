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

#ifndef _AUDIOFIFO_H
#define _AUDIOFIFO_H

#include "app.hpp"
#include <glib.h>

namespace genie {

class AudioFIFO {
public:
  AudioFIFO(App *appInstance);
  ~AudioFIFO();
  int init();
  bool isReading();

  int16_t *pcm;

protected:
  static void *loop(gpointer data);

private:
  bool running;
  bool reading;
  int fd;
  int32_t frame_length;
  App *app;
};

} // namespace genie

#endif
