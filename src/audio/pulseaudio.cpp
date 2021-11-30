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

#include "pulseaudio.hpp"
#include <string.h>

genie::PulseAudioClient::PulseAudioClient(App *app)
    : app(app), _mainloop(nullptr), _mainloop_api(nullptr), _context(nullptr),
      default_sink_name(nullptr), sink_volume(0) {}

genie::PulseAudioClient::~PulseAudioClient() {
  if (default_sink_name) {
    free(default_sink_name);
  }
  if (_context) {
    pa_context_unref(_context);
    _context = nullptr;
  }
  if (_mainloop) {
    pa_glib_mainloop_free(_mainloop);
    _mainloop = nullptr;
    _mainloop_api = nullptr;
  }
}

bool genie::PulseAudioClient::init() {
  _mainloop = pa_glib_mainloop_new(NULL);
  if (!_mainloop) {
    g_error("pa_mainloop_new() failed");
    return false;
  }

  _mainloop_api = pa_glib_mainloop_get_api(_mainloop);

  _context = pa_context_new(_mainloop_api, "Genie PulseClient");
  if (!_context) {
    g_error("pa_context_new() failed");
    return false;
  }

  if (pa_context_connect(_context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
    g_error("pa_context_connect() failed: %s",
            pa_strerror(pa_context_errno(_context)));
    return false;
  }

  pa_context_set_state_callback(_context, context_state_cb, this);
  return true;
}

void genie::PulseAudioClient::context_state_cb(pa_context *c, void *userdata) {
  PulseAudioClient *pa = (PulseAudioClient *)userdata;

  pa_operation *op = NULL;
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

void genie::PulseAudioClient::subscribe_cb(pa_context *c,
                                           pa_subscription_event_type_t type,
                                           uint32_t idx, void *userdata) {
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

void genie::PulseAudioClient::sink_info_cb(pa_context *c, const pa_sink_info *i,
                                           int eol, void *userdata) {
  PulseAudioClient *self = static_cast<PulseAudioClient *>(userdata);
  if (i) {
    float volume = (float)pa_cvolume_avg(&(i->volume)) / (float)PA_VOLUME_NORM;
    g_debug("sink volume = %.0f%%\n", volume * 100.0f);
    self->sink_volume = volume * 100.0f;
  }
}

void genie::PulseAudioClient::volume_cb(pa_context *c, int success,
                                        void *userdata) {
  if (success) {
    g_debug("Volume set\n");
  } else {
    g_debug("Volume not set\n");
  }
}

void genie::PulseAudioClient::set_volume(char *device, int channels,
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

void genie::PulseAudioClient::server_info_cb(pa_context *ctx,
                                             const pa_server_info *i,
                                             void *userdata) {
  PulseAudioClient *self = static_cast<PulseAudioClient *>(userdata);
  g_debug("pulse default sink name = %s\n", i->default_sink_name);
  self->default_sink_name = strdup(i->default_sink_name);
  self->sink_num_channels = 1;
  pa_operation *op = pa_context_get_sink_info_by_name(ctx, i->default_sink_name,
                                                      sink_info_cb, userdata);
  if (op) {
    pa_operation_unref(op);
  }
}

void genie::PulseAudioClient::set_default_volume(int volume) {
  struct pa_cvolume v;
  pa_cvolume_init(&v);
  pa_cvolume_set(&v, sink_num_channels, (65535 / 100) * volume);
  g_debug("pulse set sink %s, volume = %d\n", default_sink_name, volume);
  pa_operation *op = pa_context_set_sink_volume_by_name(
      _context, default_sink_name, &v, volume_cb, NULL);
  if (op) {
    pa_operation_unref(op);
  }
}

int genie::PulseAudioClient::get_default_volume() { return sink_volume; }
