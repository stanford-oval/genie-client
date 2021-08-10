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

#ifndef _AUDIOPLAYER_H
#define _AUDIOPLAYER_H

#include <string>
#include <gst/gst.h>
#include "app.hpp"

namespace genie
{

enum Sound_t {
    SOUND_NO_MATCH = -1,
    SOUND_MATCH = 0,
    SOUND_NEWS_INTRO = 1,
};

typedef struct {
    GstElement *pipeline;
    guint bus_watch_id;
    gchar *data;

    struct timeval tStart;
} AudioTask;

class AudioPlayer
{
public:
    AudioPlayer(App *appInstance);
    ~AudioPlayer();
    gboolean playSound(enum Sound_t id, gboolean queue = false);
    gboolean playWavFile(gchar *location);
    gboolean playOggFile(gchar *location, gboolean queue);
    gboolean playLocation(gchar *location);
    gboolean say(gchar *text);
    gboolean clean_queue();
    gboolean stop();
    gboolean resume();

private:
    void dispatch_queue();
    gboolean add_queue(GstElement *p, guint bus_id, gchar *data);
    static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data);
    static gboolean bus_call_queue(GstBus *bus, GstMessage *msg, gpointer data);
    static void on_pad_added(GstElement *element, GstPad *pad, gpointer data);
    gboolean playing;
    GQueue *playerQueue;
    AudioTask *playingTask;
    App *app;

    GstElement *pipeline;
    guint bus_watch_id;
};

}

#endif
