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
#include <json-glib/json-glib.h>
#include <string.h>

#ifdef STATIC
#include "gst/gstinitstaticplugins.h"
#endif

genie::AudioPlayer::AudioPlayer(App *appInstance) {
  app = appInstance;
  playerQueue = g_queue_new();
  playingTask = NULL;
  playing = false;

  gst_init(NULL, NULL);
#ifdef STATIC
  gst_init_static_plugins();
#endif
}

genie::AudioPlayer::~AudioPlayer() { g_queue_free(playerQueue); }

gboolean genie::AudioPlayer::bus_call_queue(GstBus *bus, GstMessage *msg,
                                            gpointer data) {
  AudioPlayer *obj = (AudioPlayer *)data;
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
        obj->app->track_processing_event(PROCESSING_END_TTS);
      }
      break;
    case GST_MESSAGE_EOS:
      g_debug("End of stream");
      obj->app->dispatch(new state::events::PlayerStreamEnd(
          obj->playingTask->type, obj->playingTask->ref_id));
      delete obj->playingTask;
      obj->playingTask = NULL;
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

      delete obj->playingTask;
      obj->playingTask = NULL;
      obj->playing = false;
      obj->dispatch_queue();
      break;
    }
    default:
      break;
  }

  return true;
}

void genie::AudioPlayer::on_pad_added(GstElement *element, GstPad *pad,
                                      gpointer data) {
  GstPad *sinkpad;
  GstElement *decoder = (GstElement *)data;
  sinkpad = gst_element_get_static_pad(decoder, "sink");
  gst_pad_link(pad, sinkpad);
  gst_object_unref(sinkpad);
}

gboolean genie::AudioPlayer::playSound(enum Sound_t id,
                                       AudioDestination destination) {
  if (id == SOUND_MATCH) {
    return playLocation("assets/match.oga", destination);
  } else if (id == SOUND_NO_MATCH) {
    return playLocation("assets/no-match.oga", destination);
  } else if (id == SOUND_NEWS_INTRO) {
    return playLocation("assets/news-intro.oga", destination);
  } else if (id == SOUND_ALARM_CLOCK_ELAPSED) {
    return playLocation("assets/alarm-clock-elapsed.oga", destination);
  }
  return false;
}

gboolean genie::AudioPlayer::playLocation(const gchar *location,
                                          AudioDestination destination) {
  if (!location || strlen(location) < 1)
    return false;

  gchar *path;
  if (*location == '/')
    path = g_strdup(location);
  else
    path = g_build_filename(g_get_current_dir(), location, nullptr);
  gchar *uri = g_strdup_printf("file://%s", path);

  gboolean ok = playURI(uri, destination);

  g_free(uri);
  g_free(path);

  return ok;
}

static const gchar *getAudioOutput(const genie::Config &config,
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

bool genie::AudioPlayer::playURI(const std::string &uri,
                                 AudioDestination destination, gint64 ref_id) {
  if (uri.empty())
    return false;

  auto sink = auto_gst_ptr<GstElement>(
      gst_element_factory_make(app->m_config->audioSink, "audio-output"),
      adopt_mode::ref_sink);
  const char *output_device = getAudioOutput(*app->m_config, destination);
  if (output_device)
    g_object_set(G_OBJECT(sink.get()), "device", output_device, NULL);

  auto pipeline = auto_gst_ptr<GstElement>(
      gst_element_factory_make("playbin", "audio-player"),
      adopt_mode::ref_sink);
  g_object_set(G_OBJECT(pipeline.get()), "uri", uri.c_str(), "audio-sink",
               sink.get(), nullptr);

  add_queue(pipeline, AudioTaskType::URI, ref_id);
  dispatch_queue();
  return true;
}

bool genie::AudioPlayer::say(const std::string &text, gint64 ref_id) {
  if (text.empty())
    return false;

  app->track_processing_event(PROCESSING_START_TTS);

  GstElement *source, *demuxer, *decoder, *conv, *sink;
  GstBus *bus;

  auto pipeline = auto_gst_ptr<GstElement>(gst_pipeline_new("audio-player"),
                                           adopt_mode::ref_sink);
  source = gst_element_factory_make("souphttpsrc", "http-source");
  decoder = gst_element_factory_make("wavparse", "wav-parser");
  sink = gst_element_factory_make(app->m_config->audioSink, "audio-output");

  if (!pipeline || !source || !decoder || !sink) {
    g_printerr("Gst element could not be created\n");
    return false;
  }

  gchar *location = g_strdup_printf("%s/en-US/voice/tts", app->m_config->nlURL);
  g_object_set(G_OBJECT(source), "location", location, NULL);
  g_free(location);
  g_object_set(G_OBJECT(source), "method", "POST", NULL);
  g_object_set(G_OBJECT(source), "content-type", "application/json", NULL);

  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);

  json_builder_set_member_name(builder, "text");
  json_builder_add_string_value(builder, text.c_str());

  json_builder_set_member_name(builder, "gender");
  json_builder_add_string_value(builder, app->m_config->audioVoice);

  json_builder_end_object(builder);

  JsonGenerator *gen = json_generator_new();
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(gen, root);
  gchar *jsonText = json_generator_to_data(gen, NULL);

  g_object_set(G_OBJECT(source), "post-data", jsonText, NULL);

  g_free(jsonText);
  json_node_free(root);
  g_object_unref(gen);
  g_object_unref(builder);

  const char *output_device =
      getAudioOutput(*app->m_config, AudioDestination::VOICE);
  if (output_device) {
    g_object_set(G_OBJECT(sink), "device", output_device, NULL);
  }

  gst_bin_add_many(GST_BIN(pipeline.get()), source, decoder, sink, NULL);

  gst_element_link_many(source, decoder, sink, NULL);

  add_queue(pipeline, AudioTaskType::SAY, ref_id);
  dispatch_queue();

  return true;
}

void genie::AudioPlayer::dispatch_queue() {
  if (!playing && !g_queue_is_empty(playerQueue)) {
    AudioTask *t = (AudioTask *)g_queue_pop_head(playerQueue);
    playingTask = t;

    // PROF_PRINT("gst pipeline started\n");
    gettimeofday(&playingTask->tStart, NULL);

    gst_element_set_state(playingTask->pipeline.get(), GST_STATE_PLAYING);
    playing = true;
  }
}

gboolean genie::AudioPlayer::add_queue(const auto_gst_ptr<GstElement> &p,
                                       AudioTaskType type, gint64 ref_id) {
  auto *bus = gst_pipeline_get_bus(GST_PIPELINE(p.get()));
  auto bus_watch_id =
      gst_bus_add_watch(bus, genie::AudioPlayer::bus_call_queue, this);
  gst_object_unref(bus);

  AudioTask *t = new AudioTask(p, bus_watch_id, type, ref_id);
  g_queue_push_tail(playerQueue, t);
  return true;
}

gboolean genie::AudioPlayer::clean_queue() {
  if (playingTask) {
    delete playingTask;
    playingTask = nullptr;
  }
  while (!g_queue_is_empty(playerQueue)) {
    AudioTask *t = (AudioTask *)g_queue_pop_head(playerQueue);
    delete t;
  }
  playing = false;
  return true;
}

gboolean genie::AudioPlayer::stop() {
  if (!playing)
    return true;
  if (playingTask && playingTask->pipeline) {
    g_print("Stop playing current pipeline\n");
    gst_element_set_state(playingTask->pipeline.get(), GST_STATE_PAUSED);
    playing = false;
  }
  clean_queue();
  return true;
}

gboolean genie::AudioPlayer::resume() {
  if (playing)
    return true;
  if (playingTask && playingTask->pipeline) {
    g_print("Resume current pipeline\n");
    gst_element_set_state(playingTask->pipeline.get(), GST_STATE_PLAYING);
    playing = true;
  }
  return true;
}

snd_mixer_elem_t *
genie::AudioPlayer::get_mixer_element(snd_mixer_t *handle,
                                      const char *selem_name) {
  snd_mixer_selem_id_t *sid;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, app->m_config->audio_output_device);
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, selem_name);
  return snd_mixer_find_selem(handle, sid);
}

int genie::AudioPlayer::adjust_playback_volume(long delta) {
  long min, max, current, updated;
  int err = 0;
  snd_mixer_t *handle = NULL;
  snd_mixer_selem_id_t *sid;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, app->m_config->audio_output_device);
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
