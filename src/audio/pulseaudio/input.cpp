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

#include "input.hpp"
#include <string.h>

genie::AudioInputPulseSimple::AudioInputPulseSimple(App *app) : app(app) {}

genie::AudioInputPulseSimple::~AudioInputPulseSimple() {
  free(pcm);
  if (pulse_handle != NULL) {
    pa_simple_free(pulse_handle);
  }
}

bool genie::AudioInputPulseSimple::init(gchar *audio_input_device,
                                        int sample_rate, int channels,
                                        int max_frame_length) {
  const pa_sample_spec config{/* format */ PA_SAMPLE_S16LE,
                              /* rate */ (uint32_t)sample_rate,
                              /* channels */ (uint8_t)channels};

  int error;
  if (!(pulse_handle = pa_simple_new(NULL, "Genie", PA_STREAM_RECORD, NULL,
                                     "record", &config, NULL, NULL, &error))) {
    g_error("pa_simple_new() failed: %s\n", pa_strerror(error));
    return false;
  }

  pcm = (int16_t *)malloc(max_frame_length * channels * sizeof(int16_t));
  if (!pcm) {
    g_error("failed to allocate memory for audio buffer\n");
    return false;
  }

  return true;
}

genie::AudioFrame
genie::AudioInputPulseSimple::read_frame(int32_t frame_length) {
  int read_frames = 0;
  int error;

  read_frames = frame_length * sizeof(int16_t);
  if (pa_simple_read(pulse_handle, pcm, read_frames, &error) < 0) {
    g_critical("pa_simple_read() failed with '%s'", pa_strerror(error));
    return AudioFrame(0);
  }
  read_frames /= sizeof(int16_t);

  if (read_frames != frame_length) {
    g_message("read %d frames instead of %d", read_frames, frame_length);
    return AudioFrame(0);
  }

  AudioFrame frame(frame_length);
  memcpy(frame.samples, pcm, frame_length * sizeof(int16_t));
  return frame;
}
