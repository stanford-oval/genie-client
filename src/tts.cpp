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
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include "tts.hpp"
#include "config.hpp"

genie::TTS::TTS(App *appInstance)
{
    app = appInstance;

    voice = app->m_config->audioVoice;
    nlUrl = app->m_config->nlURL;
    tmpFile = g_strdup("/tmp/tts.wav");
}

genie::TTS::~TTS()
{
}

#define OUTPUT_BUFFER_SIZE 8192

void genie::TTS::on_stream_splice(GObject *source, GAsyncResult *result, gpointer data)
{
    TTS *obj = reinterpret_cast<TTS *>(data);
    GError *error = NULL;
    g_output_stream_splice_finish(G_OUTPUT_STREAM(source), result, &error);
    if (error) {
        g_printerr("tts failed to download: %s\n", error->message);
        g_error_free(error);
        return;
    }

    g_object_unref(obj->session);

    obj->app->m_audioPlayer->playLocation(obj->tmpFile);

    g_unlink(obj->tmpFile);
}

void genie::TTS::on_request_sent(GObject *source, GAsyncResult *result, gpointer data)
{
    TTS *obj = reinterpret_cast<TTS *>(data);
    GError *error = NULL;
    GInputStream *in = soup_session_send_finish(SOUP_SESSION(source), result, &error);

    if (error) {
        g_printerr("tts failed to send request: %s\n", error->message);
        g_error_free(error);
    	g_object_unref(obj->session);
        return;
    }

    GFile *output_file = g_file_new_for_path(obj->tmpFile);
    GOutputStream *out = G_OUTPUT_STREAM(g_file_create(output_file, G_FILE_CREATE_NONE, NULL, &error));
    if (error) {
        g_print("tts failed to create tmpFile: %s\n", error->message);
        g_error_free(error);
        g_object_unref(in);
        g_object_unref(output_file);
    	g_object_unref(obj->session);
        return;
    }

    g_output_stream_splice_async(G_OUTPUT_STREAM(out), in,
        (GOutputStreamSpliceFlags)(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
        G_PRIORITY_DEFAULT,
        NULL,
        genie::TTS::on_stream_splice,
        data);

    g_object_unref(out);
    g_object_unref(in);
}

gboolean genie::TTS::say(gchar *text)
{
    SoupMessage *msg;

    session = soup_session_new();

    gchar *uri = g_strdup_printf("%s/en-US/voice/tts", nlUrl);
    msg = soup_message_new(SOUP_METHOD_POST, uri);
    g_free(uri);

    gchar *jsonText = g_strdup_printf("{\"text\":\"%s\",\"gender\":\"%s\"}", text, voice);
    soup_message_set_request(msg, "application/json", SOUP_MEMORY_COPY, jsonText, strlen(jsonText));
    g_free(jsonText);

    g_unlink(tmpFile);

    soup_session_send_async(session, msg, G_PRIORITY_DEFAULT, (GAsyncReadyCallback)genie::TTS::on_request_sent, this);
    g_object_unref(msg);

    return true;
}
