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
#include <string.h>

#include "../audiovolume.hpp"

genie::AudioVolumeDriverPulseAudio::AudioVolumeDriverPulseAudio(App *app)
    : app(app), _mainloop(nullptr), _mainloop_api(nullptr), _context(nullptr),
      default_sink_name(nullptr), sink_volume(0) {
  _mainloop = pa_glib_mainloop_new(NULL);
  if (!_mainloop) {
    g_error("pa_mainloop_new() failed");
    return;
  }

  _mainloop_api = pa_glib_mainloop_get_api(_mainloop);

  _context = pa_context_new(_mainloop_api, "Genie PulseClient");
  if (!_context) {
    g_error("pa_context_new() failed");
    return;
  }

  if (pa_context_connect(_context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
    g_error("pa_context_connect() failed: %s",
            pa_strerror(pa_context_errno(_context)));
    return;
  }

  pa_context_set_state_callback(_context, context_state_cb, this);
  return;
}

genie::AudioVolumeDriverPulseAudio::~AudioVolumeDriverPulseAudio() {
  if (ducking_stream != nullptr)
    pa_simple_free(ducking_stream);
  if (default_sink_name)
    free(default_sink_name);
  if (_context)
    pa_context_unref(_context);
  if (_mainloop)
    pa_glib_mainloop_free(_mainloop);
}

void genie::AudioVolumeDriverPulseAudio::duck() {
  if (ducking_stream)
    return;

  const pa_sample_spec config{/* format */ PA_SAMPLE_S16LE,
                              /* rate */ 44100,
                              /* channels */ 2};
  ducking_stream =
      pa_simple_new(NULL, "Genie-duck", PA_STREAM_PLAYBACK, NULL,
                    "Dummy Output for Ducking", &config, NULL, NULL, NULL);
}

void genie::AudioVolumeDriverPulseAudio::unduck() {
  if (!ducking_stream)
    return;

  pa_simple_free(ducking_stream);
  ducking_stream = nullptr;
}

/**
 * Linearly convert a value between two ranges.
 */
static pa_volume_t convert_to_range(pa_volume_t value, pa_volume_t from_min,
                                    pa_volume_t from_max, pa_volume_t to_min,
                                    pa_volume_t to_max) {
  return to_min +
         ((value - from_min) * (to_max - to_min) / (from_max - from_min));
}

void genie::AudioVolumeDriverPulseAudio::set_volume(int volume) {
  struct pa_cvolume v;
  pa_cvolume_init(&v);
  pa_cvolume_set(&v, sink_num_channels,
                 convert_to_range(volume, AudioVolumeController::MIN_VOLUME,
                                  AudioVolumeController::MAX_VOLUME, 0,
                                  PA_VOLUME_NORM));
  g_debug("pulse set sink %s, volume = %d\n", default_sink_name, volume);
  pa_context_set_sink_volume_by_name(_context, default_sink_name, &v, volume_cb,
                                     NULL);
}
int genie::AudioVolumeDriverPulseAudio ::get_volume() {
  return convert_to_range(sink_volume, 0, PA_VOLUME_NORM,
                          AudioVolumeController::MIN_VOLUME,
                          AudioVolumeController::MAX_VOLUME);
}

void genie::AudioVolumeDriverPulseAudio::context_state_cb(pa_context *c,
                                                          void *userdata) {
  AudioVolumeDriverPulseAudio *pa =
      static_cast<AudioVolumeDriverPulseAudio *>(userdata);

  pa_operation *op = nullptr;
  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;
    case PA_CONTEXT_READY:
      g_print("PulseAudio connection established\n");
      op = pa_context_get_server_info(c, server_info_cb, userdata);
      if (op) {
        pa_operation_unref(op);
      }
      pa_context_set_subscribe_callback(c, subscribe_cb, userdata);
      op = pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, NULL, NULL);
      if (op) {
        pa_operation_unref(op);
      }
      break;
    case PA_CONTEXT_TERMINATED:
      g_error("PulseAudio connection terminated\n");
      break;
    case PA_CONTEXT_FAILED:
    default:
      g_error("Connection failure: %s\n", pa_strerror(pa_context_errno(c)));
      break;
  }
}

void genie::AudioVolumeDriverPulseAudio::subscribe_cb(
    pa_context *c, pa_subscription_event_type_t type, uint32_t idx,
    void *userdata) {
  unsigned facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;

  pa_operation *op = NULL;
  switch (facility) {
    case PA_SUBSCRIPTION_EVENT_SINK:
      op = pa_context_get_sink_info_by_index(c, idx, sink_info_cb, userdata);
      break;
    default:
      break;
  }

  if (op) {
    pa_operation_unref(op);
  }
}

void genie::AudioVolumeDriverPulseAudio::sink_info_cb(pa_context *c,
                                                      const pa_sink_info *i,
                                                      int eol, void *userdata) {
  AudioVolumeDriverPulseAudio *self =
      static_cast<AudioVolumeDriverPulseAudio *>(userdata);
  if (i) {
    self->sink_volume = pa_cvolume_avg(&(i->volume));
    g_debug("sink volume = %d\n", (int)self->sink_volume);
  }
}

void genie::AudioVolumeDriverPulseAudio::volume_cb(pa_context *c, int success,
                                                   void *userdata) {
  if (success) {
    g_debug("Volume set\n");
  } else {
    g_debug("Volume not set\n");
  }
}

void genie::AudioVolumeDriverPulseAudio::set_volume(char *device, int channels,
                                                    int volume) {
  struct pa_cvolume v;
  pa_cvolume_init(&v);
  pa_cvolume_set(&v, channels, (65535 / 100) * volume);

  pa_operation *op =
      pa_context_set_sink_volume_by_name(_context, device, &v, volume_cb, NULL);

  if (op) {
    pa_operation_unref(op);
  }
}

void genie::AudioVolumeDriverPulseAudio::server_info_cb(pa_context *ctx,
                                                        const pa_server_info *i,
                                                        void *userdata) {
  AudioVolumeDriverPulseAudio *self =
      static_cast<AudioVolumeDriverPulseAudio *>(userdata);
  g_debug("pulse default sink name = %s\n", i->default_sink_name);
  self->default_sink_name = strdup(i->default_sink_name);
  self->sink_num_channels = 1;
  pa_operation *op = pa_context_get_sink_info_by_name(ctx, i->default_sink_name,
                                                      sink_info_cb, userdata);

  if (op) {
    pa_operation_unref(op);
  }
}
