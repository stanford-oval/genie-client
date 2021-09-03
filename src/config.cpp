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

#include "config.hpp"
#include <glib-unix.h>
#include <glib.h>
#include <string.h>

genie::Config::Config() {}

genie::Config::~Config() {
  g_free(genieURL);
  g_free(genieAccessToken);
  g_free(conversationId);
  g_free(nlURL);
  g_free(audioInputDevice);
  g_free(audioSink);
  g_free(audioOutputDeviceMusic);
  g_free(audioOutputDeviceVoice);
  g_free(audioOutputDeviceAlerts);
  g_free(audioVoice);
}

void genie::Config::load() {
  GKeyFile *key_file = g_key_file_new();
  GError *error = NULL;

  if (!g_key_file_load_from_file(key_file, "config.ini",
                                 (GKeyFileFlags)(G_KEY_FILE_KEEP_COMMENTS |
                                                 G_KEY_FILE_KEEP_TRANSLATIONS),
                                 &error)) {
    g_print("config load error: %s\n", error->message);
    g_error_free(error);
  } else {
    genieURL = g_key_file_get_string(key_file, "general", "url", &error);
    if (error || (genieURL && strlen(genieURL) == 0)) {
      genieURL = g_strdup("wss://almond.stanford.edu/me/api/conversation");
    }
    error = NULL;
    genieAccessToken =
        g_key_file_get_string(key_file, "general", "accessToken", &error);
    if (error) {
      g_error("Missing access token in config file");
      return;
    }

    error = NULL;
    nlURL = g_key_file_get_string(key_file, "general", "nlUrl", &error);
    g_debug("genieURL: %s\ngenieAccessToken: %s\nnlURL: %s\n", genieURL,
            genieAccessToken, nlURL);
    if (error) {
      g_error("Missing NLP URL in config file");
      return;
    }

    error = NULL;
    conversationId =
        g_key_file_get_string(key_file, "general", "conversationId", &error);
    if (error) {
      g_message("No conversation ID in config file, using xiaodu");
      conversationId = g_strdup("xiaodu");
      g_error_free(error);
      return;
    } else {
      g_debug("conversationId: %s\n", conversationId);
    }

    error = NULL;
    audioInputDevice =
        g_key_file_get_string(key_file, "audio", "input", &error);
    if (error) {
      g_warning("Missing audio input device in configuration file");
      audioInputDevice = g_strdup("hw:0,0");
      g_error_free(error);
      return;
    }

    error = NULL;
    audioSink = g_key_file_get_string(key_file, "audio", "sink", &error);
    if (error) {
      audioSink = g_strdup("autoaudiosink");
      audioOutputDeviceMusic = NULL;
      audioOutputDeviceVoice = NULL;
      audioOutputDeviceAlerts = NULL;
      g_error_free(error);
    } else {
      error = NULL;

      gchar *output =
          g_key_file_get_string(key_file, "audio", "output", &error);
      if (error) {
        g_error_free(error);
        output = g_strdup("hw:0,0");
      }

      error = NULL;
      audioOutputDeviceMusic =
          g_key_file_get_string(key_file, "audio", "music_output", &error);
      if (error) {
        g_error_free(error);
        audioOutputDeviceMusic = g_strdup(output);
      }

      error = NULL;
      audioOutputDeviceVoice =
          g_key_file_get_string(key_file, "audio", "voice_output", &error);
      if (error) {
        g_error_free(error);
        audioOutputDeviceVoice = g_strdup(output);
      }

      error = NULL;
      audioOutputDeviceAlerts =
          g_key_file_get_string(key_file, "audio", "alert_output", &error);
      if (error) {
        g_error_free(error);
        audioOutputDeviceAlerts = g_strdup(output);
      }

      g_free(output);
    }

    error = NULL;
    audioVoice = g_key_file_get_string(key_file, "audio", "voice", &error);
    if (error) {
      g_error_free(error);
      audioVoice = g_strdup("female");
    }

    error = NULL;
    vad_start_speaking_ms =
        g_key_file_get_integer(key_file, "vad", "start_speaking_ms", &error);
    if (error) {
      g_error_free(error);
      vad_start_speaking_ms = DEFAULT_VAD_START_SPEAKING_MS;
    }
    if (vad_start_speaking_ms < VAD_MIN_MS) {
      g_warning("CONFIG [vad] start_speaking_ms must be %d or greater, "
                "found %d. Setting to default (%d).",
                VAD_MIN_MS, vad_start_speaking_ms,
                DEFAULT_VAD_START_SPEAKING_MS);
      vad_start_speaking_ms = DEFAULT_VAD_START_SPEAKING_MS;
    }
    if (vad_start_speaking_ms > VAD_MAX_MS) {
      g_warning("CONFIG [vad] start_speaking_ms must be %d or less, "
                "found %d. Setting to default (%d).",
                VAD_MAX_MS, vad_start_speaking_ms,
                DEFAULT_VAD_START_SPEAKING_MS);
      vad_start_speaking_ms = DEFAULT_VAD_START_SPEAKING_MS;
    }

    error = NULL;
    vad_done_speaking_ms =
        g_key_file_get_integer(key_file, "vad", "done_speaking_ms", &error);
    if (error) {
      g_error_free(error);
      vad_done_speaking_ms = DEFAULT_VAD_DONE_SPEAKING_MS;
    }
    if (vad_done_speaking_ms < VAD_MIN_MS) {
      g_warning("CONFIG [vad] done_speaking_ms must be %d or greater, "
                "found %d. Setting to default (%d).",
                VAD_MIN_MS, vad_done_speaking_ms, DEFAULT_VAD_DONE_SPEAKING_MS);
      vad_done_speaking_ms = DEFAULT_VAD_DONE_SPEAKING_MS;
    }
    if (vad_done_speaking_ms > VAD_MAX_MS) {
      g_warning("CONFIG [vad] done_speaking_ms must be %d or less, "
                "found %d. Setting to default (%d).",
                VAD_MAX_MS, vad_done_speaking_ms, DEFAULT_VAD_DONE_SPEAKING_MS);
      vad_done_speaking_ms = DEFAULT_VAD_DONE_SPEAKING_MS;
    }

    error = NULL;
    vad_min_woke_ms =
        g_key_file_get_integer(key_file, "vad", "min_woke_ms", &error);
    if (error) {
      g_error_free(error);
      vad_min_woke_ms = DEFAULT_MIN_WOKE_MS;
    }
    if (vad_min_woke_ms < VAD_MIN_MS) {
      g_warning("CONFIG [vad] min_woke_ms must be %d or greater, "
                "found %d. Setting to default (%d).",
                VAD_MIN_MS, vad_min_woke_ms, DEFAULT_VAD_DONE_SPEAKING_MS);
      vad_min_woke_ms = DEFAULT_MIN_WOKE_MS;
    }
    if (vad_min_woke_ms > VAD_MAX_MS) {
      g_warning("CONFIG [vad] min_woke_ms must be %d or less, "
                "found %d. Setting to default (%d).",
                VAD_MAX_MS, vad_min_woke_ms, DEFAULT_VAD_DONE_SPEAKING_MS);
      vad_min_woke_ms = DEFAULT_MIN_WOKE_MS;
    }
  }
}
