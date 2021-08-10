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
#include <glib-unix.h>
#include <string.h>
#include "config.hpp"

genie::Config::Config()
{
}

genie::Config::~Config()
{
}

int genie::Config::load()
{
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;

    if (!g_key_file_load_from_file(key_file, "config.ini",
        (GKeyFileFlags)(G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS), &error)) {
        g_print("config load error: %s\n", error->message);
        g_error_free(error);
        return false;
    } else {
        genieURL = g_key_file_get_string(key_file, "general", "url", &error);
        if (error || (genieURL && strlen(genieURL) == 0)) {
            genieURL = (gchar *)"wss://almond.stanford.edu/me/api/conversation";
        }
        error = NULL;
        genieAccessToken = g_key_file_get_string(key_file, "general", "accessToken", &error);

        error = NULL;
        nlURL = g_key_file_get_string(key_file, "general", "nlUrl", &error);
        g_debug("genieURL: %s\ngenieAccessToken: %s\nnlURL: %s\n",
            genieURL, genieAccessToken, nlURL);

        error = NULL;
        conversationId = g_key_file_get_string(key_file, "general", "conversationId", &error);
        g_debug("conversationId: %s\n", conversationId);

        error = NULL;
        audioInputDevice = g_key_file_get_string(key_file, "audio", "input", &error);

        error = NULL;
        audioSink = g_key_file_get_string(key_file, "audio", "sink", &error);
        if (error) {
            audioSink = (gchar *)"autoaudiosink";
            audioOutputDevice = NULL;
        } else {
            error = NULL;
            audioOutputDevice = g_key_file_get_string(key_file, "audio", "output", &error);
        }

        error = NULL;
        audioVoice = g_key_file_get_string(key_file, "audio", "voice", &error);

        g_debug("audioInputDevice: %s\nvoice: %s\n", audioInputDevice, audioVoice);
        if (error) {
            g_print("config load warning: %s\n", error->message);
            g_error_free(error);
        }
        return true;
    }
}
