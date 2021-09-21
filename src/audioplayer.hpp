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
#include "utils/autoptrs.hpp"

#include <alsa/asoundlib.h>
#include <gst/gst.h>
#include <string>
#include <memory>
#include <queue>

namespace genie {

struct AudioTask {
  auto_gst_ptr<GstElement> pipeline;
  guint bus_watch_id;
  AudioTaskType type;
  gint64 ref_id;

  struct timeval tStart;

  AudioTask(const auto_gst_ptr<GstElement> &pipeline, guint bus_watch_id,
            AudioTaskType type, gint64 ref_id)
      : pipeline(pipeline), bus_watch_id(bus_watch_id), type(type),
        ref_id(ref_id) {}

  AudioTask(const AudioTask&) = delete;
  AudioTask(AudioTask&&) = delete;

  ~AudioTask() {
    gst_element_set_state(pipeline.get(), GST_STATE_NULL);
    if (bus_watch_id)
      g_source_remove(bus_watch_id);
  }
};

enum class AudioDestination { VOICE, MUSIC, ALERT };

class AudioPlayer {
public:
  AudioPlayer(App *appInstance);
  gboolean playSound(enum Sound_t id,
                     AudioDestination destination = AudioDestination::ALERT);
  bool play_url(const std::string &url,
                AudioDestination destination = AudioDestination::MUSIC,
                gint64 ref_id = -1);
  gboolean playLocation(const gchar *location,
                        AudioDestination destination = AudioDestination::MUSIC);
  bool say(const std::string &text, gint64 ref_id = -1);
  gboolean clean_queue();
  gboolean stop();
  gboolean resume();
  long get_volume();
  void set_volume(long volume);
  int adjust_playback_volume(long delta);
  int increment_playback_volume();
  int decrement_playback_volume();

private:
  void dispatch_queue();
  gboolean add_queue(const auto_gst_ptr<GstElement> &p, AudioTaskType type,
                     gint64 ref_id);
  static gboolean bus_call_queue(GstBus *bus, GstMessage *msg, gpointer data);
  static void on_pad_added(GstElement *element, GstPad *pad, gpointer data);
  gboolean playing;
  std::queue<std::unique_ptr<AudioTask>> player_queue;
  std::unique_ptr<AudioTask> playing_task;
  App *app;
  snd_mixer_elem_t *get_mixer_element(snd_mixer_t *handle,
                                      const char *selem_name);
};

} // namespace genie
