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

#include "utils/autoptrs.hpp"
#include "leds.hpp"
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
  g_free(audio_backend);
  g_free(ssl_ca_file);
  g_free(leds_path);
  g_free(leds_type);
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

int genie::Config::get_leds_effect_string(GKeyFile *key_file, const char *section,
                                 const char *key, const char *default_value) {
  GError *error = NULL;
  gchar *value = g_key_file_get_string(key_file, section, key, &error);
  if (error != NULL) {
    g_warning("Failed to load [%s] %s from config file, using default '%s'",
              section, key, default_value);
    g_error_free(error);
    value = strdup(default_value);
  }

  int i;
  if (strcmp(value, "none") == 0) {
    i = (int)LedsAnimation_t::None;
  } else if (strcmp(value, "solid") == 0) {
    i = (int)LedsAnimation_t::Solid;
  } else if (strcmp(value, "circular") == 0) {
    i = (int)LedsAnimation_t::Circular;
  } else if (strcmp(value, "pulse") == 0) {
    i = (int)LedsAnimation_t::Pulse;
  } else {
    g_warning("Failed to parse [%s] %s from config file, using 'none'", section,
              key);
    i = (int)LedsAnimation_t::None;
  }
  free(value);
  return i;
}

int genie::Config::get_dec_color_from_hex_string(GKeyFile *key_file, const char *section,
                                 const char *key, const char *default_value) {
  GError *error = NULL;
  gchar *value = g_key_file_get_string(key_file, section, key, &error);
  if (error != NULL) {
    g_warning("Failed to load [%s] %s from config file, using default '%s'",
              section, key, default_value);
    g_error_free(error);
    value = strdup(default_value);
  }

  unsigned short r, g, b;
  if (sscanf(value, "%02hx%02hx%02hx", &r, &g, &b) != 3) {
    g_warning("Failed to parse [%s] %s from config file, using default '%s'",
              section, key, default_value);
    value = strdup(default_value);
    sscanf(value, "%02hx%02hx%02hx", &r, &g, &b);
  }
  int i = (r << 16) + (g << 8) + b;
  // g_debug("converted hex %s in %d\n", value, i);
  free(value);
  return i;
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
  ssize_t value = g_key_file_get_integer(key_file, section, key, &error);
  if (error != NULL) {
    g_warning("Failed to load [%s] %s from config file, using default %zd",
              section, key, default_value);
    g_error_free(error);
    return default_value;
  }
  if (value < 0) {
    g_warning("Failed to load [%s] %s from config file. Value must be 0 or "
              "greater, found %zd. Using default %zd",
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

  ssize_t value = get_size(key_file, section, key, default_value);

  if (value < (ssize_t)min) {
    g_warning("CONFIG [%s] %s must be %zd or greater, found %zd. "
              "Setting to default (%zd).",
              section, key, min, value, default_value);
    return default_value;
  }

  if (value > (ssize_t)max) {
    g_warning("CONFIG [%s] %s must be %zd or less, found %zd. "
              "Setting to default (%zd).",
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

  error = NULL;
  audio_backend = g_key_file_get_string(key_file, "audio", "backend", &error);
  if (error) {
    g_error_free(error);
    audio_backend = g_strdup("alsa");
  }

  audio_output_device =
      get_string(key_file, "audio", "output", DEFAULT_AUDIO_OUTPUT_DEVICE);

  error = NULL;
  audio_input_stereo2mono =
      g_key_file_get_boolean(key_file, "audio", "stereo2mono", &error);
  if (error) {
    g_error_free(error);
    audio_input_stereo2mono = false;
  }

  // Echo Cancellation
  // =========================================================================

  error = NULL;
  audio_ec_enabled =
      g_key_file_get_boolean(key_file, "ec", "enabled", &error);
  if (error) {
    g_error_free(error);
    audio_ec_enabled = false;
  }

  error = NULL;
  audio_ec_loopback =
      g_key_file_get_boolean(key_file, "ec", "loopback", &error);
  if (error) {
    g_error_free(error);
    audio_ec_loopback = false;
  }

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

  // Leds
  // =========================================================================

  error = NULL;
  leds_enabled =
      g_key_file_get_boolean(key_file, "leds", "enabled", &error);
  if (error) {
    leds_enabled = false;
    g_error_free(error);
  }

  if (leds_enabled) {
    error = NULL;
    leds_type = g_key_file_get_string(key_file, "leds", "type", &error);
    if (error) {
      g_warning("Missing leds control type in configuration file, disabling");
      leds_enabled = false;
      g_error_free(error);
    }

    error = NULL;
    leds_path = g_key_file_get_string(key_file, "leds", "path", &error);
    if (error) {
      g_warning("Missing leds path in configuration file, disabling");
      leds_enabled = false;
      g_error_free(error);
    }

    leds_starting_effect = get_leds_effect_string(
        key_file, "leds", "starting_effect", DEFAULT_LEDS_STARTING_EFFECT);
    leds_starting_color = get_dec_color_from_hex_string(
        key_file, "leds", "starting_color", DEFAULT_LEDS_STARTING_COLOR);
    leds_sleeping_effect = get_leds_effect_string(
        key_file, "leds", "sleeping_effect", DEFAULT_LEDS_SLEEPING_EFFECT);
    leds_sleeping_color = get_dec_color_from_hex_string(
        key_file, "leds", "sleeping_color", DEFAULT_LEDS_SLEEPING_COLOR);
    leds_listening_effect = get_leds_effect_string(
        key_file, "leds", "listening_effect", DEFAULT_LEDS_LISTENING_EFFECT);
    leds_listening_color = get_dec_color_from_hex_string(
        key_file, "leds", "listening_color", DEFAULT_LEDS_LISTENING_COLOR);
    leds_processing_effect = get_leds_effect_string(
        key_file, "leds", "processing_effect", DEFAULT_LEDS_PROCESSING_EFFECT);
    leds_processing_color = get_dec_color_from_hex_string(
        key_file, "leds", "processing_color", DEFAULT_LEDS_PROCESSING_COLOR);
    leds_saying_effect = get_leds_effect_string(
        key_file, "leds", "saying_effect", DEFAULT_LEDS_SAYING_EFFECT);
    leds_saying_color = get_dec_color_from_hex_string(
        key_file, "leds", "saying_color", DEFAULT_LEDS_SAYING_COLOR);
    leds_error_effect = get_leds_effect_string(
        key_file, "leds", "error_effect", DEFAULT_LEDS_ERROR_EFFECT);
    leds_error_color = get_dec_color_from_hex_string(
        key_file, "leds", "error_color", DEFAULT_LEDS_ERROR_COLOR);
    leds_net_error_effect = get_leds_effect_string(
        key_file, "leds", "net_error_effect", DEFAULT_LEDS_NET_ERROR_EFFECT);
    leds_net_error_color = get_dec_color_from_hex_string(
        key_file, "leds", "net_error_color", DEFAULT_LEDS_NET_ERROR_COLOR);
  }

  // System
  // =========================================================================

  dns_controller_enabled =
      g_key_file_get_boolean(key_file, "system", "dns", nullptr);

  ssl_ca_file = g_key_file_get_string(key_file, "system", "ssl_ca_file", nullptr);

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
