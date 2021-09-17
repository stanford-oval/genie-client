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

#include "autoptrs.hpp"
#include <memory>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::Config"

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
  g_free(audio_output_device);
  g_free(leds_path);
}

gchar *genie::Config::get_string(GKeyFile *key_file, const char *section,
                                 const char *key, const char *default_value) {
  GError *error = NULL;
  gchar *value = g_key_file_get_string(key_file, section, key, &error);
  if (error != NULL) {
    g_warning("Failed to load [%s] %s from config file, using default '%s'",
              section, key, default_value);
    g_error_free(error);
    return strdup(default_value);
  }
  return value;
}

/**
 * @brief Helper to get a _size_ (unsigned integer) from the config file,
 * returning a `default_value` if there is an error or the retrieved value is
 * negative.
 *
 * Gets an integer, checks it's zero or higher, and casts it
 */
size_t genie::Config::get_size(GKeyFile *key_file, const char *section,
                               const char *key, const size_t default_value) {
  GError *error = NULL;
  gint value = g_key_file_get_integer(key_file, section, key, &error);
  if (error != NULL) {
    g_warning("Failed to load [%s] %s from config file, using default %d",
              section, key, default_value);
    g_error_free(error);
    return default_value;
  }
  if (value < 0) {
    g_warning("Failed to load [%s] %s from config file. Value must be 0 or "
              "greater, found %d. Using default %d",
              section, key, value, default_value);
    g_error_free(error);
    return default_value;
  }
  return (size_t)value;
}

/**
 * @brief Like the `get_size` helper, but also checks that the retrieved value
 * is within a `min` and `max` (inclusive).
 */
size_t genie::Config::get_bounded_size(GKeyFile *key_file, const char *section,
                                       const char *key,
                                       const size_t default_value,
                                       const size_t min, const size_t max) {
  g_assert(min <= max);
  g_assert(default_value >= min);
  g_assert(default_value <= max);

  size_t value = get_size(key_file, section, key, default_value);

  if (value < min) {
    g_warning("CONFIG [%s] %s must be %d or greater, found %d. "
              "Setting to default (%d).",
              section, key, min, value, default_value);
    return default_value;
  }

  if (value > max) {
    g_warning("CONFIG [%s] %s must be %d or less, found %d. "
              "Setting to default (%d).",
              section, key, max, value, default_value);
    return default_value;
  }

  return value;
}

double genie::Config::get_double(GKeyFile *key_file, const char *section,
                                 const char *key, const double default_value) {
  GError *error = NULL;
  double value = g_key_file_get_double(key_file, section, key, &error);
  if (error != NULL) {
    g_warning("Failed to load [%s] %s from config file, using default %f",
              section, key, default_value);
    g_error_free(error);
    return default_value;
  }
  return value;
}

double genie::Config::get_bounded_double(GKeyFile *key_file,
                                         const char *section, const char *key,
                                         const double default_value,
                                         const double min, const double max) {
  g_assert(min <= max);
  g_assert(default_value >= min);
  g_assert(default_value <= max);

  double value = get_double(key_file, section, key, default_value);

  if (value < min) {
    g_warning("CONFIG [%s] %s must be %f or greater, found %f. "
              "Setting to default (%f).",
              section, key, min, value, default_value);
    return default_value;
  }

  if (value > max) {
    g_warning("CONFIG [%s] %s must be %f or less, found %f. "
              "Setting to default (%f).",
              section, key, max, value, default_value);
    return default_value;
  }

  return value;
}

void genie::Config::load() {
  std::unique_ptr<GKeyFile, fn_deleter<GKeyFile, g_key_file_free>>
      auto_key_file(g_key_file_new());
  GKeyFile *key_file = auto_key_file.get();
  GError *error = NULL;

  if (!g_key_file_load_from_file(key_file, "config.ini",
                                 (GKeyFileFlags)(G_KEY_FILE_KEEP_COMMENTS |
                                                 G_KEY_FILE_KEEP_TRANSLATIONS),
                                 &error)) {
    g_critical("config load error: %s\n", error->message);
    g_error_free(error);
    return;
  }

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
  } else {
    g_debug("conversationId: %s\n", conversationId);
  }

  // Audio
  // =========================================================================

  error = NULL;
  audioInputDevice = g_key_file_get_string(key_file, "audio", "input", &error);
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

    gchar *output = g_key_file_get_string(key_file, "audio", "output", &error);
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
  audioOutputFIFO =
      g_key_file_get_string(key_file, "audio", "output_fifo", &error);
  if (error) {
    g_error_free(error);
    audioOutputFIFO = g_strdup("/tmp/playback.fifo");
  }

  error = NULL;
  audioVoice = g_key_file_get_string(key_file, "audio", "voice", &error);
  if (error) {
    g_error_free(error);
    audioVoice = g_strdup("female");
  }

  audio_output_device =
      get_string(key_file, "audio", "output", DEFAULT_AUDIO_OUTPUT_DEVICE);

  // Picovoice
  // =========================================================================

  pv_library_path =
      get_string(key_file, "picovoice", "library", DEFAULT_PV_LIBRARY_PATH);

  pv_model_path =
      get_string(key_file, "picovoice", "model", DEFAULT_PV_MODEL_PATH);

  pv_keyword_path =
      get_string(key_file, "picovoice", "keyword", DEFAULT_PV_KEYWORD_PATH);

  pv_sensitivity = (float)get_bounded_double(
      key_file, "picovoice", "sensitivity", DEFAULT_PV_SENSITIVITY, 0, 1);

  pv_wake_word_pattern = get_string(key_file, "picovoice", "wake_word_pattern",
                                    DEFAULT_PV_WAKE_WORD_PATTERN);

  // Sounds
  // =========================================================================

  sound_wake = get_string(key_file, "sound", "wake", DEFAULT_SOUND_WAKE);
  sound_no_input =
      get_string(key_file, "sound", "no_input", DEFAULT_SOUND_NO_INPUT);
  sound_too_much_input = get_string(key_file, "sound", "too_much_input",
                                    DEFAULT_SOUND_TOO_MUCH_INPUT);
  sound_news_intro =
      get_string(key_file, "sound", "news_intro", DEFAULT_SOUND_NEWS_INTRO);
  sound_alarm_clock_elapsed =
      get_string(key_file, "sound", "alarm_clock_elapsed",
                 DEFAULT_SOUND_ALARM_CLOCK_ELAPSED);
  sound_working =
      get_string(key_file, "sound", "working", DEFAULT_SOUND_WORKING);
  sound_stt_error =
      get_string(key_file, "sound", "stt_error", DEFAULT_SOUND_STT_ERROR);

  // System
  // =========================================================================

  dns_controller_enabled =
      g_key_file_get_boolean(key_file, "system", "dns", nullptr);

  leds_path = g_key_file_get_string(key_file, "system", "leds", nullptr);

  // Voice Activity Detection (VAD)
  // =========================================================================

  vad_start_speaking_ms =
      get_bounded_size(key_file, "vad", "start_speaking_ms",
                       DEFAULT_VAD_START_SPEAKING_MS, VAD_MIN_MS, VAD_MAX_MS);

  vad_done_speaking_ms =
      get_bounded_size(key_file, "vad", "done_speaking_ms",
                       DEFAULT_VAD_DONE_SPEAKING_MS, VAD_MIN_MS, VAD_MAX_MS);

  vad_input_detected_noise_ms = get_bounded_size(
      key_file, "vad", "input_detected_noise_ms",
      DEFAULT_VAD_INPUT_DETECTED_NOISE_MS, VAD_MIN_MS, VAD_MAX_MS);
}
