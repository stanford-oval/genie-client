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

#include "audioplayer.hpp"

#include <glib.h>
#include <gst/gst.h>
#include <string.h>

#ifdef STATIC
#include "gst/gstinitstaticplugins.h"
#endif

static const gchar *get_audio_output(const genie::Config &config,
                                     genie::AudioDestination destination) {
  switch (destination) {
    case genie::AudioDestination::MUSIC:
      return config.audioOutputDeviceMusic;
    case genie::AudioDestination::ALERT:
      return config.audioOutputDeviceAlerts;
    case genie::AudioDestination::VOICE:
      return config.audioOutputDeviceVoice;
    default:
      g_warn_if_reached();
      return config.audioOutputDeviceMusic;
  }
}

void genie::URLAudioTask::start() {
  g_object_set(G_OBJECT(pipeline.get()), "uri", url.c_str(), nullptr);

  // PROF_PRINT("gst pipeline started\n");
  gettimeofday(&t_start, NULL);
  gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
}

void genie::SayAudioTask::start() {
  auto_gobject_ptr<JsonGenerator> gen(json_generator_new(), adopt_mode::owned);
  JsonNode *root = json_builder_get_root(json.get());
  json_generator_set_root(gen.get(), root);
  gchar *jsonText = json_generator_to_data(gen.get(), NULL);

  g_object_set(G_OBJECT(soupsrc.get()), "post-data", jsonText, NULL);

  g_free(jsonText);

  // PROF_PRINT("gst pipeline started\n");
  gettimeofday(&t_start, NULL);
  gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
}

genie::AudioPlayer::AudioPlayer(App *appInstance)
    : app(appInstance), playing(false) {
  gst_init(NULL, NULL);
#ifdef STATIC
  gst_init_static_plugins();
#endif

  init_say_pipeline();
  init_url_pipeline();
}

void genie::AudioPlayer::init_say_pipeline() {
  auto pipeline = auto_gst_ptr<GstElement>(gst_pipeline_new("audio-player-say"),
                                           adopt_mode::ref_sink);
  soupsrc = auto_gst_ptr<GstElement>(
      gst_element_factory_make("souphttpsrc", "http-source"),
      adopt_mode::ref_sink);
  auto decoder = gst_element_factory_make("wavparse", "wav-parser");
  auto sink = gst_element_factory_make(app->config->audioSink, "audio-output");

  if (!pipeline || !soupsrc || !decoder || !sink) {
    g_error("Gst element could not be created\n");
  }

  gchar *location = g_strdup_printf("%s/en-US/voice/tts", app->config->nlURL);
  g_object_set(G_OBJECT(soupsrc.get()), "location", location, "method", "POST",
               "content-type", "application/json", NULL);
  g_free(location);

  const char *output_device =
      get_audio_output(*app->config, AudioDestination::VOICE);
  if (output_device) {
    g_object_set(G_OBJECT(sink), "device", output_device, NULL);
  }

  gst_bin_add_many(GST_BIN(pipeline.get()), soupsrc.get(), decoder, sink, NULL);
  gst_element_link_many(soupsrc.get(), decoder, sink, NULL);

  say_pipeline.init(this, pipeline);
}

void genie::AudioPlayer::init_url_pipeline() {
  auto sink = auto_gst_ptr<GstElement>(
      gst_element_factory_make(app->config->audioSink, "audio-output"),
      adopt_mode::ref_sink);
  const char *output_device =
      get_audio_output(*app->config, AudioDestination::MUSIC /* FIXME */);
  if (output_device)
    g_object_set(G_OBJECT(sink.get()), "device", output_device, NULL);

  auto pipeline = auto_gst_ptr<GstElement>(
      gst_element_factory_make("playbin", "audio-player-url"),
      adopt_mode::ref_sink);
  g_object_set(G_OBJECT(pipeline.get()), "audio-sink", sink.get(), nullptr);

  url_pipeline.init(this, pipeline);
}

void genie::AudioPlayer::PipelineState::init(
    AudioPlayer *self, const auto_gst_ptr<GstElement> &pipeline) {
  this->pipeline = pipeline;
  auto *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline.get()));
  bus_watch_id =
      gst_bus_add_watch(bus, genie::AudioPlayer::bus_call_queue, self);
  gst_object_unref(bus);
}

gboolean genie::AudioPlayer::bus_call_queue(GstBus *bus, GstMessage *msg,
                                            gpointer data) {
  AudioPlayer *obj = static_cast<AudioPlayer *>(data);
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_STREAM_STATUS:
      // PROF_PRINT("Stream status changed\n");

      // Dig into the status message... we want to know when the audio
      // stream starts, 'cause that's what we consider the when we deliver
      // to the user -- it ends both the TTS window and entire run of
      // processing tracking.
      GstStreamStatusType type;
      GstElement *owner;
      gst_message_parse_stream_status(msg, &type, &owner);

      // By printing all status events and timing when the audio starts
      // it seems like we want the "enter" type event.
      //
      // There are actually _two_ of them fired, and we want the second --
      // which is convinient for us, because we can track the event _both_
      // times, with the second time over-writing the first, which results
      // in the desired state.
      if (type == GST_STREAM_STATUS_TYPE_ENTER) {
        obj->app->track_processing_event(ProcessingEventType::END_TTS);
      }
      break;
    case GST_MESSAGE_EOS:
      g_message("End of stream");
      obj->app->dispatch(new state::events::PlayerStreamEnd(
          obj->playing_task->type, obj->playing_task->ref_id));
      obj->playing_task->stop();
      obj->playing_task = nullptr;
      obj->playing = false;
      obj->dispatch_queue();
      break;
    case GST_MESSAGE_ERROR: {
      gchar *debug;
      GError *error = NULL;
      gst_message_parse_error(msg, &error, &debug);
      g_free(debug);

      g_printerr("Error: %s\n", error->message);
      g_error_free(error);

      obj->playing_task->stop();
      obj->playing_task = nullptr;
      obj->playing = false;
      obj->dispatch_queue();
      break;
    }
    default:
      break;
  }

  return true;
}

gboolean genie::AudioPlayer::play_sound(enum Sound_t id,
                                        AudioDestination destination) {
  switch (id) {
    case Sound_t::WAKE:
      return play_location(app->config->sound_wake, destination);
    case Sound_t::NO_INPUT:
      return play_location(app->config->sound_no_input, destination);
    case Sound_t::TOO_MUCH_INPUT:
      return play_location(app->config->sound_too_much_input, destination);
    case Sound_t::NEWS_INTRO:
      return play_location(app->config->sound_news_intro, destination);
    case Sound_t::ALARM_CLOCK_ELAPSED:
      return play_location(app->config->sound_alarm_clock_elapsed, destination);
    case Sound_t::WORKING:
      return play_location(app->config->sound_working, destination);
    case Sound_t::STT_ERROR:
      return play_location(app->config->sound_stt_error, destination);
  }
  return false;
}

gboolean genie::AudioPlayer::play_location(const gchar *location,
                                           AudioDestination destination) {
  if (!location || strlen(location) < 1)
    return false;

  gchar *path;
  if (*location == '/')
    path = g_strdup(location);
  else
    path = g_build_filename(g_get_current_dir(), "assets", location, nullptr);
  gchar *uri = g_strdup_printf("file://%s", path);

  gboolean ok = play_url(uri, destination);

  g_free(uri);
  g_free(path);

  return ok;
}

bool genie::AudioPlayer::play_url(const std::string &uri,
                                  AudioDestination destination, gint64 ref_id) {
  if (uri.empty())
    return false;

  g_message("Queueing %s for playback", uri.c_str());

  player_queue.push(
      std::make_unique<URLAudioTask>(url_pipeline.pipeline, uri, ref_id));
  dispatch_queue();
  return true;
}

bool genie::AudioPlayer::say(const std::string &text, gint64 ref_id) {
  if (text.empty())
    return false;

  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);

  json_builder_set_member_name(builder, "text");
  json_builder_add_string_value(builder, text.c_str());

  json_builder_set_member_name(builder, "gender");
  json_builder_add_string_value(builder, app->config->audioVoice);

  json_builder_end_object(builder);

  player_queue.push(std::make_unique<SayAudioTask>(
      say_pipeline.pipeline, soupsrc,
      auto_gobject_ptr<JsonBuilder>(builder, adopt_mode::owned), ref_id));
  dispatch_queue();

  return true;
}

void genie::AudioPlayer::dispatch_queue() {
  if (!playing && !player_queue.empty()) {
    std::unique_ptr<AudioTask> &task = player_queue.front();
    playing_task = std::move(task);
    player_queue.pop();

    playing_task->start();
    playing = true;
  }
}

gboolean genie::AudioPlayer::clean_queue() {
  if (playing_task)
    playing_task->stop();
  playing_task.reset();
  while (!player_queue.empty()) {
    player_queue.pop();
  }
  playing = false;
  return true;
}

gboolean genie::AudioPlayer::stop() {
  if (!playing)
    return true;
  clean_queue();
  return true;
}

snd_mixer_elem_t *
genie::AudioPlayer::get_mixer_element(snd_mixer_t *handle,
                                      const char *selem_name) {
  snd_mixer_selem_id_t *sid;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, app->config->audio_output_device);
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, selem_name);
  return snd_mixer_find_selem(handle, sid);
}

void genie::AudioPlayer::set_volume(long volume) {
  int err = 0;
  snd_mixer_t *handle = NULL;
  snd_mixer_selem_id_t *sid;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, app->config->audio_output_device);
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, "LINEOUT volume");
  snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);

  snd_mixer_selem_set_playback_volume_all(elem, volume);

  g_message("Updated playback volume to %ld", volume);

  snd_mixer_close(handle);
}

int genie::AudioPlayer::adjust_playback_volume(long delta) {
  long min, max, current, updated;
  int err = 0;
  snd_mixer_t *handle = NULL;
  snd_mixer_selem_id_t *sid;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, app->config->audio_output_device);
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, "LINEOUT volume");
  snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);

  err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
  if (err != 0) {
    g_warning("Error getting playback volume range, code=%d", err);
    snd_mixer_close(handle);
    return err;
  }

  err =
      snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &current);
  if (err != 0) {
    g_warning("Error getting current playback volume, code=%d", err);
    snd_mixer_close(handle);
    return err;
  }

  updated = current + delta;

  if (updated > max) {
    g_message(
        "Can not adjust playback volume to %ld, max is %ld. Capping at max",
        updated, max);
    updated = max;
  } else if (updated < min) {
    g_message(
        "Can not adjust playback volume to %ld; min is %ld. Capping at min",
        updated, min);
    updated = min;
  }

  if (updated == current) {
    g_message("Volume already at %ld", current);
    snd_mixer_close(handle);
    return 0;
  }

  snd_mixer_selem_set_playback_volume_all(elem, updated);

  g_message("Updated playback volume to %ld", updated);

  snd_mixer_close(handle);
  return 0;
}

int genie::AudioPlayer::increment_playback_volume() {
  return adjust_playback_volume(1);
}

int genie::AudioPlayer::decrement_playback_volume() {
  return adjust_playback_volume(-1);
}
