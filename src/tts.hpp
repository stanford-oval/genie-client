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

#ifndef _TTS_H
#define _TTS_H

#include <glib.h>
#include <libsoup/soup.h>
#include "audioplayer.hpp"
#include "app.hpp"

namespace genie
{

class TTS
{
public:
    TTS(App *appInstance);
    ~TTS();
    gboolean say(gchar *text);

protected:
    static void on_stream_splice(GObject *source, GAsyncResult *result, gpointer data);
    static void on_request_sent(GObject *source, GAsyncResult *result, gpointer data);

private:
    App *app;
    gchar *voice;
    gchar *tmpFile;
    gchar *nlUrl;
    SoupSession *session;
};

}

#endif
