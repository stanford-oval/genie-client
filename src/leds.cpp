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

#include "leds.hpp"
#include <fcntl.h>
#include <glib-unix.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

genie::Leds::Leds(App *appInstance) {
  app = appInstance;
  initialized = false;
  update_timer_circular = false;
  update_timer_pulse = false;
}

genie::Leds::~Leds() {
  free(ctrl_path_all);
  free(ctrl_path_brightness);
  free(leds);
  set_user(false);
}

int genie::Leds::init() {
  if (!app->config->leds_enabled) return false;

  if (strcmp(app->config->leds_type, "aw") == 0) {
    led_count = 12;
    ctrl_path_base = app->config->leds_path;
    ctrl_path_brightness = g_strdup_printf("%s/brightness", ctrl_path_base);
    ctrl_path_all = g_strdup_printf("%s/set_all_led_color", ctrl_path_base);
  } else {
    g_warning("Invalid leds type in configuration file");
    return false;
  }

  leds = (int *)malloc(led_count * sizeof(int));

  max_brightness = get_brightness_internal(true);
  brightness = get_brightness_internal(false);

  base_color = 0;
  animate_internal(LedsAnimation_t::None);
  if (set_user(true)) {
    initialized = true;
    return true;
  }

  return false;
}

void genie::Leds::animate(LedsState_t state) {
  if (!app->config->leds_enabled || !initialized) return;

  switch (state) {
    case LedsState_t::Starting:
      g_debug("leds starting\n");
      animate_internal((LedsAnimation_t)app->config->leds_starting_effect,
                       app->config->leds_starting_color);
      break;
    case LedsState_t::Sleeping:
      g_debug("leds sleeping\n");
      animate_internal((LedsAnimation_t)app->config->leds_sleeping_effect,
                       app->config->leds_sleeping_color);
      break;
    case LedsState_t::Listening:
      g_debug("leds listening\n");
      animate_internal((LedsAnimation_t)app->config->leds_listening_effect,
                       app->config->leds_listening_color);
      break;
    case LedsState_t::Processing:
      g_debug("leds processing\n");
      animate_internal((LedsAnimation_t)app->config->leds_processing_effect,
                       app->config->leds_processing_color);
      break;
    case LedsState_t::Saying:
      g_debug("leds saying\n");
      animate_internal((LedsAnimation_t)app->config->leds_saying_effect,
                       app->config->leds_saying_color);
      break;
    case LedsState_t::Error:
      g_debug("leds error\n");
      animate_internal((LedsAnimation_t)app->config->leds_error_effect,
                       app->config->leds_error_color);
      break;
    case LedsState_t::NetError:
      g_debug("leds error\n");
      animate_internal((LedsAnimation_t)app->config->leds_net_error_effect,
                       app->config->leds_net_error_color);
      break;
  }
}

void genie::Leds::animate_internal(LedsAnimation_t style, int color, int duration) {
  switch (style) {
    case LedsAnimation_t::None:
      solid(0);
      break;
    case LedsAnimation_t::Solid:
      solid(color);
      break;
    case LedsAnimation_t::Circular:
      circular(color);
      break;
    case LedsAnimation_t::Pulse:
      solid(color);
      pulse();
      break;
  }
}

bool genie::Leds::set_user(bool enabled) {
  char path[256];
  snprintf(path, sizeof(path) - 1, "%s/user_space", ctrl_path_base);
  int fd = open(path, O_WRONLY);
  if (fd > 0) {
    write(fd, enabled ? "1" : "0", 1);
  } else {
    return false;
  }
  close(fd);
  return true;
}

bool genie::Leds::set_brightness(int level) {
  if (level > max_brightness) return false;

  char buffer[16];
  snprintf(buffer, sizeof(buffer) - 1, "%d", level);
  int fd = open(ctrl_path_brightness, O_WRONLY);
  if (fd > 0) {
    write(fd, buffer, strlen(buffer));
  } else {
    return false;
  }
  close(fd);
  return true;
}

int genie::Leds::get_brightness_internal(bool max = false) {
  char path[256], buffer[64];
  snprintf(path, sizeof(path) - 1, "%s/%sbrightness", ctrl_path_base, max ? "max_" : "");
  int fd = open(path, O_RDONLY);
  if (fd > 0) {
    read(fd, buffer, sizeof(buffer) - 1);
  } else {
    return false;
  }
  close(fd);
  int result = -1;
  sscanf(buffer, "%d", &result);
  return result;
}

void genie::Leds::solid(int color) {
  if (update_timer_circular) {
    update_timer_circular = false;
    usleep(100);
  }
  clear(color);
  set_leds();
}

void genie::Leds::clear(int color) {
  for (int i = 0; i < led_count; i++) {
    leds[i] = color;
  }
}

bool genie::Leds::set_leds() {
  char buffer[128];
  for (int i = 0, j = 0; i < led_count; i++) {
    j += snprintf (buffer + j, sizeof(buffer) - j, "%d ", leds[i]);
  }
  int fd = open(ctrl_path_all, O_WRONLY);
  if (fd > 0) {
    write(fd, buffer, strlen(buffer));
    // g_debug("write-set-leds %s\n", buffer);
  } else {
    return false;
  }
  close(fd);
  return true;
}

gboolean genie::Leds::update_circular(gpointer data) {
  Leds *obj = (Leds *)data;

  if (!obj->update_timer_circular) return false;

  obj->leds[obj->step_circular] = obj->base_color;
  obj->step_circular++;
  if (obj->step_circular == obj->led_count) {
    obj->step_circular = 0;
  }
  obj->leds[obj->step_circular] = obj->color_circular;

  obj->set_leds();

  return obj->update_timer_circular;
}

gboolean genie::Leds::update_pulse(gpointer data) {
  Leds *obj = (Leds *)data;

  if (!obj->update_timer_pulse) {
    obj->set_brightness(obj->brightness);
    return false;
  }

  if (obj->bright_direction) {
    if (obj->step_bright < obj->max_brightness) {
      obj->step_bright += 10;
    } else {
      obj->step_bright = obj->max_brightness;
      obj->bright_direction = 0;
    }
  } else {
    if (obj->step_bright > 0) {
      obj->step_bright -= 10;
    } else {
      obj->step_bright = 0;
      obj->bright_direction = 1;
    }
  }
  obj->set_brightness(obj->step_bright);

  return obj->update_timer_pulse;
}

void genie::Leds::circular(int color) {
  if (update_timer_circular) {
    color_circular = color;
    return;
  }
  if (update_timer_pulse) {
    update_timer_pulse = false;
  }

  step_circular = 0;
  color_circular = color;
  update_timer_circular = true;
  g_timeout_add(100, update_circular, this);
}

void genie::Leds::pulse() {
  if (update_timer_pulse) return;
  if (update_timer_circular) {
    update_timer_circular = false;
    usleep(100);
  }

  step_bright = brightness;
  bright_direction = true;
  set_brightness(0);
  update_timer_pulse = true;
  g_timeout_add(50, update_pulse, this);
}
