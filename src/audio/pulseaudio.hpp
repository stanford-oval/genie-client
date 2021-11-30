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

#include <thread>
#include <pulse/glib-mainloop.h>
#include <pulse/pulseaudio.h>
#include <pulse/error.h>

namespace genie {

class PulseAudioClient {
public:
  PulseAudioClient(App *app);
  ~PulseAudioClient();
  bool init();
  void set_volume(char *device, int channels, int volume);
  void set_default_volume(int volume);
  int get_default_volume();

protected:
  static void volume_cb(pa_context *ctx, int success, void *userdata);
  static void context_state_cb(pa_context *ctx, void *userdata);
  static void server_info_cb(pa_context *ctx, const pa_server_info *i,
                             void *userdata);
  static void subscribe_cb(pa_context *c, pa_subscription_event_type_t type,
                           uint32_t idx, void *userdata);
  static void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol,
                           void *userdata);

private:
  // initialized once and never overwritten
  App *const app;

  std::thread pa_thread;
  pa_glib_mainloop *_mainloop;
  pa_mainloop_api *_mainloop_api;
  pa_context *_context;

  char *default_sink_name;
  int sink_num_channels;
  int sink_volume;
};

} // namespace genie
