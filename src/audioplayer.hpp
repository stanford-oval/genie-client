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
#include <json-glib/json-glib.h>
#include <memory>
#include <queue>
#include <string>

namespace genie {

enum class AudioDestination { VOICE, MUSIC, ALERT };

class AudioTask {
protected:
  auto_gobject_ptr<GstElement> pipeline;
  struct timeval t_start;

public:
  AudioTaskType type;
  gint64 ref_id;

  AudioTask(const auto_gobject_ptr<GstElement> &pipeline, AudioTaskType type,
            gint64 ref_id)
      : pipeline(pipeline), type(type), ref_id(ref_id) {}
  AudioTask(const AudioTask &) = delete;
  AudioTask(AudioTask &&) = delete;

  void stop() { gst_element_set_state(pipeline.get(), GST_STATE_READY); }

  virtual ~AudioTask() = default;
  virtual void start() = 0;
};

class URLAudioTask : public AudioTask {
  std::string url;

public:
  URLAudioTask(const auto_gobject_ptr<GstElement> &pipeline,
               const std::string &url, gint64 ref_id)
      : AudioTask(pipeline, AudioTaskType::URL, ref_id), url(url) {}

  void start() override;
};

class SayAudioTask : public AudioTask {
  auto_gobject_ptr<GstElement> soupsrc;
  std::string text;
  const std::string &base_tts_url;
  const char *voice;
  bool soup_has_post_data;

public:
  SayAudioTask(const auto_gobject_ptr<GstElement> &pipeline,
               const auto_gobject_ptr<GstElement> &soupsrc,
               const std::string &text, const std::string &base_tts_url,
               const char *voice, bool soup_has_post_data, gint64 ref_id)
      : AudioTask(pipeline, AudioTaskType::SAY, ref_id), soupsrc(soupsrc),
        text(text), base_tts_url(base_tts_url), voice(voice),
        soup_has_post_data(soup_has_post_data) {}

  void start() override;

private:
  void say_get();
  void say_post();
};

class AudioPlayer {
public:
  AudioPlayer(App *appInstance);
  gboolean play_sound(enum Sound_t id,
                      AudioDestination destination = AudioDestination::ALERT);
  bool play_url(const std::string &url,
                AudioDestination destination = AudioDestination::MUSIC,
                gint64 ref_id = -1);
  gboolean
  play_location(const gchar *location,
                AudioDestination destination = AudioDestination::MUSIC);
  bool say(const std::string &text, gint64 ref_id = -1);
  gboolean clean_queue();
  gboolean stop();
  long get_volume();
  void set_volume(long volume);
  int adjust_playback_volume(long delta);
  int increment_playback_volume();
  int decrement_playback_volume();

private:
  struct PipelineState {
    auto_gobject_ptr<GstElement> pipeline;
    guint bus_watch_id = 0;

    PipelineState() = default;
    ~PipelineState() {
      g_source_remove(bus_watch_id);
      gst_element_set_state(pipeline.get(), GST_STATE_NULL);
    }

    void init(AudioPlayer *self, const auto_gobject_ptr<GstElement> &pipeline);
  } say_pipeline, url_pipeline;
  auto_gobject_ptr<GstElement> soupsrc;
  App *const app;
  std::string base_tts_url;
  bool soup_has_post_data;
  bool playing;

  void init_say_pipeline();
  void init_url_pipeline();

  void dispatch_queue();
  static gboolean bus_call_queue(GstBus *bus, GstMessage *msg, gpointer data);
  std::queue<std::unique_ptr<AudioTask>> player_queue;
  std::unique_ptr<AudioTask> playing_task;

  snd_mixer_elem_t *get_mixer_element(snd_mixer_t *handle,
                                      const char *selem_name);
};

} // namespace genie
