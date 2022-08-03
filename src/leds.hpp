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

namespace genie {

enum class LedsState_t {
  Starting,
  Sleeping,
  Listening,
  Processing,
  Saying,
  Config,
  Error,
  NetError,
  Disabled
};

enum class LedsAnimation_t { None, Solid, Circular, Pulse };

class Leds {
public:
  Leds(App *appInstance);
  ~Leds();
  int init();

  void animate(enum LedsState_t state);

protected:
  void animate_internal(LedsAnimation_t style, int color = 0, int duration = 0);
  bool set_user(bool enabled);
  int get_brightness_internal(bool max);
  bool set_brightness(int level);
  bool set_leds();

  void clear(int color = 0);
  void solid(int color);
  void circular(int color);
  void pulse();

  static gboolean update_circular(gpointer data);
  static gboolean update_pulse(gpointer data);

private:
  bool initialized;
  App *app;
  gchar *ctrl_path_base;
  gchar *ctrl_path_brightness;
  gchar *ctrl_path_all;
  int *leds;
  int led_count;
  int max_brightness;

  int base_color;
  int brightness;

  int step_circular;
  int color_circular;
  bool update_timer_circular;

  int step_bright;
  int bright_direction;
  bool update_timer_pulse;
};

} // namespace genie
