// -*- mode: cpp; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include <glib.h>
#include <gst/gst.h>
#include <string.h>

#include "app.hpp"
#include "audioplayer.hpp"

#ifdef STATIC
#include "gst/gstinitstaticplugins.h"
#endif

genie::AudioPlayer::AudioPlayer(App *appInstance)
{
    app = appInstance;
    playerQueue = g_queue_new();
    playingTask = NULL;
    playing = false;

    gst_init(NULL, NULL);
#ifdef STATIC
    gst_init_static_plugins();
#endif
}

genie::AudioPlayer::~AudioPlayer()
{
    g_queue_free(playerQueue);
}

gboolean genie::AudioPlayer::bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
    AudioPlayer *obj = (AudioPlayer *)data;
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_STREAM_STATUS:
            g_debug("Stream status changed\n");
            // PROF_PRINT("Stream status changed\n");
            break;
        case GST_MESSAGE_EOS:
            g_debug("End of stream\n");
            gst_element_set_state(obj->pipeline.get(), GST_STATE_NULL);
            gst_object_unref(GST_OBJECT(obj->pipeline.get()));
            g_source_remove(obj->bus_watch_id);
            break;
        case GST_MESSAGE_ERROR: {
            gchar *debug;
            GError *error = NULL;
            gst_message_parse_error(msg, &error, &debug);
            g_free(debug);

            g_printerr("Error: %s\n", error->message);
            g_error_free(error);

            gst_element_set_state(obj->pipeline.get(), GST_STATE_NULL);
            gst_object_unref(GST_OBJECT(obj->pipeline.get()));
            g_source_remove(obj->bus_watch_id);
            break;
        }
        default:
            break;
    }

    return true;
}

gboolean genie::AudioPlayer::bus_call_queue(GstBus *bus, GstMessage *msg, gpointer data)
{
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
            g_debug("End of stream\n");
            // PROF_TIME_DIFF("End of stream", obj->playingTask->tStart);
            obj->app->track_processing_event(PROCESSING_FINISH);
            gst_element_set_state(obj->playingTask->pipeline, GST_STATE_NULL);
            gst_object_unref(GST_OBJECT(obj->playingTask->pipeline));
            g_source_remove(obj->playingTask->bus_watch_id);
            g_free(obj->playingTask);
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

            gst_element_set_state(obj->playingTask->pipeline, GST_STATE_NULL);
            gst_object_unref(GST_OBJECT(obj->playingTask->pipeline));
            g_source_remove(obj->playingTask->bus_watch_id);
            g_free(obj->playingTask);
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

void genie::AudioPlayer::on_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
    GstPad *sinkpad;
    GstElement *decoder = (GstElement *)data;
    sinkpad = gst_element_get_static_pad(decoder, "sink");
    gst_pad_link(pad, sinkpad);
    gst_object_unref(sinkpad);
}

gboolean genie::AudioPlayer::playSound(enum Sound_t id, gboolean queue)
{
    if (id == SOUND_MATCH) {
        return playLocation((gchar *)"assets/match.oga", queue);
    } else if (id == SOUND_NO_MATCH) {
        return playLocation((gchar *)"assets/no-match.oga", queue);
    } else if (id == SOUND_NEWS_INTRO) {
        return playLocation((gchar *)"assets/news-intro.oga", queue);
    }
    return false;
}

gboolean genie::AudioPlayer::playLocation(const gchar *location, gboolean queue = false) {
    if (!location || strlen(location) < 1)
        return false;

    gchar* path;
    if (*location == '/')
        path = g_strdup(location);
    else
        path = g_build_filename(g_get_current_dir(), location, nullptr);
    gchar* uri = g_strdup_printf("file://%s", path);

    gboolean ok = playURI(uri, queue);

    g_free(uri);
    g_free(path);

    return ok;
}

gboolean genie::AudioPlayer::playURI(const gchar *uri, gboolean queue = false)
{
    if (!uri || strlen(uri) < 1)
        return false;

    auto sink = auto_gst_ptr<GstElement>(
        gst_element_factory_make(app->m_config->audioSink, "audio-output"),
        adopt_mode::ref_sink);
    if (app->m_config->audioOutputDevice)
        g_object_set(G_OBJECT(sink.get()), "device", app->m_config->audioOutputDevice, NULL);

    pipeline = auto_gst_ptr<GstElement>(gst_element_factory_make("playbin", "audio-player"), adopt_mode::ref_sink);
    g_object_set(G_OBJECT(pipeline.get()),
        "uri", uri,
        "audio-sink", sink.get(),
        nullptr);

    auto bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline.get()));
    bus_watch_id = gst_bus_add_watch(bus, genie::AudioPlayer::bus_call, this);
    gst_object_unref(bus);

    g_print("Now playing: %s\n", uri);
    gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);

    return true;
}

gboolean genie::AudioPlayer::say(const gchar *text)
{
    if (!text || strlen(text) < 1)
        return false;

    app->track_processing_event(PROCESSING_START_TTS);

    GstElement *source, *demuxer, *decoder, *conv, *sink;
    GstBus *bus;

    pipeline = gst_pipeline_new("audio-player");
    source = gst_element_factory_make("souphttpsrc", "http-source");
    decoder = gst_element_factory_make("wavparse", "wav-parser");
    sink = gst_element_factory_make(app->m_config->audioSink, "audio-output");

    if (!pipeline || !source || !decoder || !sink) {
        g_printerr("Gst element could not be created\n");
        return -1;
    }

    gchar *location = g_strdup_printf("%s/en-US/voice/tts", app->m_config->nlURL);
    g_object_set(G_OBJECT(source), "location", location, NULL);
    g_free(location);
    g_object_set(G_OBJECT(source), "method", "POST", NULL);
    g_object_set(G_OBJECT(source), "content-type", "application/json", NULL);
    gchar *jsonText = g_strdup_printf("{\"text\":\"%s\",\"gender\":\"%s\"}", text, app->m_config->audioVoice);
    g_object_set(G_OBJECT(source), "post-data", jsonText, NULL);
    g_free(jsonText);
    if (app->m_config->audioOutputDevice) {
        g_object_set(G_OBJECT(sink), "device", app->m_config->audioOutputDevice, NULL);
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline.get()));
    bus_watch_id = gst_bus_add_watch(bus, genie::AudioPlayer::bus_call_queue, this);
    gst_object_unref(bus);

    gst_bin_add_many(GST_BIN(pipeline.get()), source, decoder, sink, NULL);

    gst_element_link_many(source, decoder, sink, NULL);

    add_queue(pipeline.get(), bus_watch_id, text);
    dispatch_queue();

    return true;
}

void genie::AudioPlayer::dispatch_queue()
{
    if (!playing && !g_queue_is_empty(playerQueue)) {
        AudioTask *t = (AudioTask *)g_queue_pop_head(playerQueue);
        playingTask = t;

        // PROF_PRINT("gst pipeline started\n");
        gettimeofday(&playingTask->tStart, NULL);

        PROF_PRINT("Now playing: %s\n", playingTask->data);
        gst_element_set_state(playingTask->pipeline, GST_STATE_PLAYING);
        playing = true;
    }
}

gboolean genie::AudioPlayer::add_queue(GstElement *p, guint bus_id, const gchar *data)
{
    AudioTask *t = g_new(AudioTask, 1);
    t->pipeline = p;
    t->bus_watch_id = bus_id;
    t->data = data;
    g_queue_push_tail(playerQueue, t);
    return true;
}

gboolean genie::AudioPlayer::clean_queue()
{
    if (playing) {
        gst_element_set_state(playingTask->pipeline, GST_STATE_NULL);
        gst_object_unref(GST_OBJECT(playingTask->pipeline));
        g_source_remove(playingTask->bus_watch_id);
        playing = false;
    }
    while (!g_queue_is_empty(playerQueue)) {
        AudioTask *t = (AudioTask *)g_queue_pop_head(playerQueue);
        gst_element_set_state(t->pipeline, GST_STATE_NULL);
        gst_object_unref(GST_OBJECT(t->pipeline));
        g_source_remove(t->bus_watch_id);
        g_free(t);
    }
    return true;
}

gboolean genie::AudioPlayer::stop()
{
    if (!playing) return true;
    if (playingTask && playingTask->pipeline) {
        g_print("Stop playing current pipeline\n");
        gst_element_set_state(playingTask->pipeline, GST_STATE_PAUSED);
        playing = false;
    }
    return true;
}

gboolean genie::AudioPlayer::resume()
{
    if (playing) return true;
    if (playingTask && playingTask->pipeline) {
        g_print("Resume current pipeline\n");
        gst_element_set_state(playingTask->pipeline, GST_STATE_PLAYING);
        playing = true;
    }
    return true;
}
