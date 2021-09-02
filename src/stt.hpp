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

#ifndef _STT_H
#define _STT_H

#include "app.hpp"
#include <glib.h>
#include <libsoup/soup.h>
#include "audioinput.hpp"
#include "audioplayer.hpp"
#include "wsclient.hpp"

namespace genie
{

class STT
{
public:
    STT(App *appInstance);
    ~STT();
    int init();
    void send_frame(AudioFrame *frame);
    void send_done();
    int connect();

private:
    void setConnection(SoupWebsocketConnection *conn);
    static void on_connection(SoupSession *session, GAsyncResult *res, gpointer data);
    static void on_message(SoupWebsocketConnection *conn, gint type, GBytes *message, gpointer data);
    static void on_close(SoupWebsocketConnection *conn, gpointer data);
    void dispatch_frame(AudioFrame *frame);
    gboolean is_connection_open();

private:
    App *app;
    gchar *url;
    gboolean connected;
    SoupWebsocketConnection *wconn;
    gboolean acceptStream;
    GQueue *queue;
    
    struct timeval tConnect;
    struct timeval tFirstFrame;
    struct timeval tLastFrame;
    gboolean firstFrame;
};

}

#endif
