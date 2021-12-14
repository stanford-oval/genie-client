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

#include "evinput.hpp"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

genie::EVInput::EVInput(App *app) : app(app) {}

genie::EVInput::~EVInput() { g_source_unref(source); }

gboolean genie::EVInput::event_prepare(GSource *source, gint *timeout) {
  *timeout = -1;
  return false;
}

gboolean genie::EVInput::event_check(GSource *source) {
  InputEventSource *event_source = (InputEventSource *)source;
  gboolean rc;
  rc = (event_source->event_poll_fd.revents & G_IO_IN);
  return rc;
}

gboolean genie::EVInput::event_dispatch(GSource *g_source, GSourceFunc callback,
                                        gpointer user_data) {
  EVInput *ev_input = static_cast<EVInput *>(user_data);
  InputEventSource *source = (InputEventSource *)g_source;
  input_event ev;
  int rc = 0;

  rc = libevdev_next_event(source->device, LIBEVDEV_READ_FLAG_NORMAL, &ev);
  while (rc != -EAGAIN) {
    if (rc == 0) {
      g_debug("Event type %d (%s), code %d (%s), value %d", ev.type,
              libevdev_event_type_get_name(ev.type), ev.code,
              libevdev_event_code_get_name(ev.type, ev.code), ev.value);
      // Dispatch key events
      if (ev.type == EV_KEY && ev.value == 0) {
        // For key constant names/values see:
        //
        // https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h
        //
        switch (ev.code) {
          case KEY_VOLUMEUP:
            ev_input->app->dispatch(new state::events::AdjustVolume(1));
            break;
          case KEY_VOLUMEDOWN:
            ev_input->app->dispatch(new state::events::AdjustVolume(-1));
            break;
          case KEY_MUTE:
            ev_input->app->dispatch(new state::events::ToggleDisabled());
            break;
          case KEY_PLAYPAUSE:
            // ev_input->app->dispatch(new state::events::TogglePlayback());
            ev_input->app->dispatch(new state::events::Panic());
            break;
          default:
            g_warning("Unhandled button up event, code=%d", ev.code);
            break;
        }
      }
    } else {
      g_print("source read failed %s\n", strerror(-rc));
      break;
    }

    rc = libevdev_next_event(source->device, LIBEVDEV_READ_FLAG_NORMAL, &ev);
  }

  return callback(user_data);
}

gboolean genie::EVInput::callback(gpointer user_data) {
  return G_SOURCE_CONTINUE;
}

int genie::EVInput::init() {
  if (!app->config->buttons_enabled)
    return false;

  int fd, rc = 1;

  source = g_source_new(&event_funcs, sizeof(InputEventSource));
  InputEventSource *event_source = (InputEventSource *)source;

  fd = open(app->config->evinput_device, O_RDONLY | O_NONBLOCK);
  if (fd == -1) {
    g_print("Failed to init libevdev (%s)\n", strerror(-fd));
    return false;
  }

  event_source->device = NULL;
  event_source->event_poll_fd.fd = fd;
  event_source->event_poll_fd.events = G_IO_IN;
  rc = libevdev_new_from_fd(fd, &event_source->device);
  if (rc < 0) {
    g_print("Failed to init libevdev (%s)\n", strerror(-rc));
    return false;
  }

  g_debug("Input device name: \"%s\"\n",
          libevdev_get_name(event_source->device));
  g_debug("Input device ID: bus %#x vendor %#x product %#x\n",
          libevdev_get_id_bustype(event_source->device),
          libevdev_get_id_vendor(event_source->device),
          libevdev_get_id_product(event_source->device));
  g_debug("Evdev version: %x\n",
          libevdev_get_driver_version(event_source->device));
  g_debug("Input device name: \"%s\"\n",
          libevdev_get_name(event_source->device));
  g_debug("Phys location: %s\n", libevdev_get_phys(event_source->device));
  g_debug("Uniq identifier: %s\n", libevdev_get_uniq(event_source->device));

  g_source_set_priority(source, G_PRIORITY_DEFAULT);
  g_source_add_poll(source, &event_source->event_poll_fd);
  g_source_set_can_recurse(source, true);
  g_source_set_callback(source, (GSourceFunc)callback, this, NULL);
  g_source_attach(source, NULL);

  return true;
}
