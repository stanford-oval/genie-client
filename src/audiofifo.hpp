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
#include "pa_ringbuffer.h"
#include <atomic>
#include <glib.h>

namespace genie {

class AudioFIFO {
public:
  static const size_t BYTES_PER_SAMPLE = sizeof(int16_t);
  static const size_t MAX_FRAME_LENGTH__SAMPLES = 512;
  static const size_t MAX_FRAME_LENGTH__BYTES =
      MAX_FRAME_LENGTH__SAMPLES * BYTES_PER_SAMPLE;
  static const size_t BUFFER_SIZE__FRAMES = 1024;
  static const size_t SAMPLE__NS = 62500;
  static const size_t SLEEP__US = 8000;

  AudioFIFO(App *appInstance);
  ~AudioFIFO();
  int init();
  PaUtilRingBuffer ring_buffer;

protected:
  static void *loop(gpointer data);

private:
  std::atomic_bool running;
  int fd;
  int16_t *pcm;
  App *app;
  
  void write_to_ring(AudioFrame* frame);
};

} // namespace genie

#endif
