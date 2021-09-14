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
#include "autoptrs.hpp"

#include <alsa/asoundlib.h>
#include <gst/gst.h>
#include <string>

namespace genie {

struct AudioTask {
  auto_gst_ptr<GstElement> pipeline;
  guint bus_watch_id;
  gchar *data;

  struct timeval tStart;

  AudioTask(const auto_gst_ptr<GstElement> &_pipeline, guint _bus_watch_id,
            const gchar *_data)
      : pipeline(_pipeline), bus_watch_id(_bus_watch_id),
        data(g_strdup(_data)) {}

  ~AudioTask() {
    gst_element_set_state(pipeline.get(), GST_STATE_NULL);
    if (bus_watch_id)
      g_source_remove(bus_watch_id);
    g_free(data);
  }
};

enum class AudioDestination { VOICE, MUSIC, ALERT };

class AudioPlayer {
public:
  AudioPlayer(App *appInstance);
  ~AudioPlayer();
  gboolean playSound(enum Sound_t id,
                     AudioDestination destination = AudioDestination::ALERT);
  gboolean playURI(const gchar *uri,
                   AudioDestination destination = AudioDestination::MUSIC);
  gboolean playLocation(const gchar *location,
                        AudioDestination destination = AudioDestination::MUSIC);
  gboolean say(const gchar *text);
  gboolean clean_queue();
  gboolean stop();
  gboolean resume();
  long get_volume();
  int adjust_playback_volume(long delta);
  int increment_playback_volume();
  int decrement_playback_volume();

private:
  void dispatch_queue();
  gboolean add_queue(const auto_gst_ptr<GstElement> &p, const gchar *data);
  static gboolean bus_call_queue(GstBus *bus, GstMessage *msg, gpointer data);
  static void on_pad_added(GstElement *element, GstPad *pad, gpointer data);
  gboolean playing;
  GQueue *playerQueue;
  AudioTask *playingTask;
  App *app;
  snd_mixer_elem_t *get_mixer_element(snd_mixer_t *handle,
                                      const char *selem_name);
};

} // namespace genie
