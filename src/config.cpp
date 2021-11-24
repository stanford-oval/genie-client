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

#include "leds.hpp"
#include "utils/autoptrs.hpp"
#include <memory>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::Config"

genie::Config::Config() {}

genie::Config::~Config() {
  g_free(genie_url);
  g_free(genie_access_token);
  g_free(conversation_id);
  g_free(nl_url);
  g_free(locale);
  g_free(asset_dir);
  g_free(audio_input_device);
  g_free(audio_sink);
  g_free(audio_output_device_music);
  g_free(audio_output_device_voice);
  g_free(audio_output_device_alerts);
  g_free(audio_voice);
  g_free(audio_output_device);
  g_free(audio_backend);
  g_free(sound_wake);
  g_free(sound_no_input);
  g_free(sound_too_much_input);
  g_free(sound_news_intro);
  g_free(sound_alarm_clock_elapsed);
  g_free(sound_working);
  g_free(sound_stt_error);
  g_free(pv_model_path);
  g_free(pv_keyword_path);
  g_free(pv_wake_word_pattern);
  g_free(proxy);
  g_free(ssl_ca_file);
  g_free(evinput_device);
  g_free(leds_path);
  g_free(leds_type);
  g_free(cache_dir);

  g_key_file_unref(key_file);
}

gchar *genie::Config::get_string(const char *section, const char *key,
                                 const char *default_value) {
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

int genie::Config::get_leds_effect_string(const char *section, const char *key,
                                          const char *default_value) {
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

int genie::Config::get_dec_color_from_hex_string(const char *section,
                                                 const char *key,
                                                 const char *default_value) {
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
size_t genie::Config::get_size(const char *section, const char *key,
                               const size_t default_value) {
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
size_t genie::Config::get_bounded_size(const char *section, const char *key,
                                       const size_t default_value,
                                       const size_t min, const size_t max) {
  g_assert(min <= max);
  g_assert(default_value >= min);
  g_assert(default_value <= max);

  ssize_t value = get_size(section, key, default_value);

  if (value < (ssize_t)min) {
    g_warning("CONFIG [%s] %s must be %zd or greater, found %zd. "
              "Setting to minimum.",
              section, key, min, value);
    return min;
  }

  if (value > (ssize_t)max) {
    g_warning("CONFIG [%s] %s must be %zd or less, found %zd. "
              "Setting to maximum.",
              section, key, max, value);
    return max;
  }

  return value;
}

double genie::Config::get_double(const char *section, const char *key,
                                 const double default_value) {
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

double genie::Config::get_bounded_double(const char *section, const char *key,
                                         const double default_value,
                                         const double min, const double max) {
  g_assert(min <= max);
  g_assert(default_value >= min);
  g_assert(default_value <= max);

  double value = get_double(section, key, default_value);

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

bool genie::Config::get_bool(const char *section, const char *key,
                             const bool default_value) {
  GError *error = NULL;
  gboolean value = g_key_file_get_boolean(key_file, section, key, &error);
  if (error != NULL) {
    g_warning("Failed to load [%s] %s from config file, using default %s",
              section, key, default_value ? "true" : "false");
    g_error_free(error);
    return default_value;
  }
  return static_cast<bool>(value);
}

static genie::AuthMode get_auth_mode(GKeyFile *key_file) {
  GError *error = nullptr;
  ;
  char *value = g_key_file_get_string(key_file, "general", "auth_mode", &error);
  if (value == nullptr) {
    g_warning("Failed to load [general] auth_mode from config file, using "
              "default 'none'");
    g_error_free(error);
    return genie::AuthMode::NONE;
  }

  if (strcmp(value, "none") == 0) {
    g_free(value);
    return genie::AuthMode::NONE;
  } else if (strcmp(value, "bearer") == 0) {
    g_free(value);
    return genie::AuthMode::BEARER;
  } else if (strcmp(value, "cookie") == 0) {
    g_free(value);
    return genie::AuthMode::COOKIE;
  } else if (strcmp(value, "home-assistant") == 0) {
    g_free(value);
    return genie::AuthMode::HOME_ASSISTANT;
  } else {
    g_warning("Failed to load [general] auth_mode from config file, using "
              "default 'none'");
    g_free(value);
    return genie::AuthMode::NONE;
  }
}

void genie::Config::save() {
  GError *error = NULL;
  g_key_file_save_to_file(key_file, "config.ini", &error);
  if (error) {
    g_critical("Failed to save configuration file to disk: %s\n",
               error->message);
    g_error_free(error);
  }
}

void genie::Config::load() {
  key_file = g_key_file_new();

  GError *error = NULL;
  if (!g_key_file_load_from_file(key_file, "config.ini",
                                 (GKeyFileFlags)(G_KEY_FILE_KEEP_COMMENTS |
                                                 G_KEY_FILE_KEEP_TRANSLATIONS),
                                 &error)) {
    if (error->domain != G_FILE_ERROR || error->code != G_FILE_ERROR_NOENT)
      g_critical("config load error: %s\n", error->message);
    g_error_free(error);
    return;
  }

  asset_dir = get_string("general", "assets_dir", pkglibdir "/assets");

  genie_url = g_key_file_get_string(key_file, "general", "url", &error);
  if (error || !genie_url || strlen(genie_url) == 0) {
    genie_url = g_strdup(DEFAULT_GENIE_URL);
  }
  g_clear_error(&error);

  retry_interval =
      get_size("general", "retry_interval", DEFAULT_WS_RETRY_INTERVAL);

  connect_timeout =
      get_size("general", "connect_timeout", DEFAULT_CONNECT_TIMEOUT);

  auth_mode = get_auth_mode(key_file);
  if (auth_mode != AuthMode::NONE) {
    genie_access_token =
        g_key_file_get_string(key_file, "general", "accessToken", &error);
    if (error) {
      g_error("Missing access token in config file");
      return;
    }
  } else {
    genie_access_token = nullptr;
  }

  error = NULL;
  nl_url = g_key_file_get_string(key_file, "general", "nlUrl", &error);
  if (error) {
    nl_url = g_strdup(DEFAULT_NLP_URL);
    g_clear_error(&error);
  }

  error = NULL;
  locale = g_key_file_get_string(key_file, "general", "locale", &error);
  if (error) {
    locale = g_strdup(DEFAULT_LOCALE);
    g_clear_error(&error);
  }

  g_debug("genieURL: %s\ngenieAccessToken: %s\nnlURL: %s\nlocale: %s\n",
          genie_url, genie_access_token, nl_url, locale);

  error = NULL;
  locale = g_key_file_get_string(key_file, "general", "locale", &error);
  if (error) {
    locale = g_strdup(DEFAULT_LOCALE);
    g_clear_error(&error);
  }

  g_debug("genieURL: %s\ngenieAccessToken: %s\nnlURL: %s\nlocale: %s\n",
          genie_url, genie_access_token, nl_url, locale);

  error = NULL;
  conversation_id =
      g_key_file_get_string(key_file, "general", "conversationId", &error);
  if (error) {
    g_message("No conversation ID in config file, using genie-client");
    conversation_id = g_strdup("genie-client");
    g_clear_error(&error);
  } else {
    g_debug("conversationId: %s\n", conversation_id);
  }

  // Audio
  // =========================================================================

  error = NULL;

  audio_backend = get_string("audio", "backend", "pulse");
  if (strcmp(audio_backend, "pulse") == 0) {
    audio_input_device = nullptr;
    audio_output_fifo = nullptr;
    audio_input_stereo2mono = false;
    audio_sink = g_strdup("pulsesink");

    audio_output_device =
        get_string("audio", "output", DEFAULT_PULSE_AUDIO_OUTPUT_DEVICE);
    audio_output_device_music = g_strdup(audio_output_device);
    audio_output_device_voice = g_strdup(audio_output_device);
    audio_output_device_alerts = g_strdup(audio_output_device);
  } else if (strcmp(audio_backend, "alsa") == 0) {
    audio_input_device =
        g_key_file_get_string(key_file, "audio", "input", &error);
    if (error) {
      g_warning("Missing audio input device in configuration file");
      audio_input_device = g_strdup("hw:0,0");
      g_clear_error(&error);
    }

    audio_sink = g_strdup("alsasink");
    audio_output_device =
        get_string("audio", "output", DEFAULT_ALSA_AUDIO_OUTPUT_DEVICE);

    error = NULL;
    audio_output_device_music =
        g_key_file_get_string(key_file, "audio", "music_output", &error);
    if (error) {
      g_error_free(error);
      audio_output_device_music = g_strdup(audio_output_device);
    }

    error = NULL;
    audio_output_device_voice =
        g_key_file_get_string(key_file, "audio", "voice_output", &error);
    if (error) {
      g_error_free(error);
      audio_output_device_voice = g_strdup(audio_output_device);
    }

    error = NULL;
    audio_output_device_alerts =
        g_key_file_get_string(key_file, "audio", "alert_output", &error);
    if (error) {
      g_error_free(error);
      audio_output_device_alerts = g_strdup(audio_output_device);
    }

    error = NULL;
    audio_output_fifo =
        g_key_file_get_string(key_file, "audio", "output_fifo", &error);
    if (error) {
      g_error_free(error);
      audio_output_fifo = g_strdup("/tmp/playback.fifo");
    }

    error = NULL;
    audio_input_stereo2mono =
        g_key_file_get_boolean(key_file, "audio", "stereo2mono", &error);
    if (error) {
      g_error_free(error);
      audio_input_stereo2mono = false;
    }
  } else {
    g_error("Invalid audio backend %s", audio_backend);
    return;
  }

  audio_voice = get_string("audio", "voice", DEFAULT_VOICE);

  // Echo Cancellation
  // =========================================================================

  error = NULL;
  audio_ec_enabled = g_key_file_get_boolean(key_file, "ec", "enabled", &error);
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

  // Hacks
  // =========================================================================

  hacks_wake_word_verification = get_bool("hacks", "wake_word_verification",
                                          DEFAULT_HACKS_WAKE_WORD_VERIFICATION);

  hacks_surpress_repeated_notifs =
      get_bool("hacks", "surpress_repeated_notifs",
               DEFAULT_HACKS_SURPRESS_REPEATED_NOTIFS);

  hacks_dns_server =
      get_string("hacks", "dns_server", DEFAULT_HACKS_DNS_SERVER);

  // Picovoice
  // =========================================================================

  pv_model_path = get_string("picovoice", "model", DEFAULT_PV_MODEL_PATH);

  pv_keyword_path = get_string("picovoice", "keyword", DEFAULT_PV_KEYWORD_PATH);

  pv_sensitivity = (float)get_bounded_double("picovoice", "sensitivity",
                                             DEFAULT_PV_SENSITIVITY, 0, 1);

  pv_wake_word_pattern = get_string("picovoice", "wake_word_pattern",
                                    DEFAULT_PV_WAKE_WORD_PATTERN);

  // Sounds
  // =========================================================================

  sound_wake = get_string("sound", "wake", DEFAULT_SOUND_WAKE);
  sound_no_input = get_string("sound", "no_input", DEFAULT_SOUND_NO_INPUT);
  sound_too_much_input =
      get_string("sound", "too_much_input", DEFAULT_SOUND_TOO_MUCH_INPUT);
  sound_news_intro =
      get_string("sound", "news_intro", DEFAULT_SOUND_NEWS_INTRO);
  sound_alarm_clock_elapsed = get_string("sound", "alarm_clock_elapsed",
                                         DEFAULT_SOUND_ALARM_CLOCK_ELAPSED);
  sound_working = get_string("sound", "working", DEFAULT_SOUND_WORKING);
  sound_stt_error = get_string("sound", "stt_error", DEFAULT_SOUND_STT_ERROR);

  // Buttons
  // =========================================================================
  error = NULL;
  buttons_enabled =
      g_key_file_get_boolean(key_file, "buttons", "enabled", &error);
  if (error) {
    buttons_enabled = true;
    g_error_free(error);
  }

  if (buttons_enabled) {
    evinput_device = get_string("buttons", "evinput_dev", DEFAULT_EVINPUT_DEV);
  } else {
    evinput_device = nullptr;
  }

  // Leds
  // =========================================================================

  error = NULL;
  leds_enabled = g_key_file_get_boolean(key_file, "leds", "enabled", &error);
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

    leds_starting_effect = get_leds_effect_string("leds", "starting_effect",
                                                  DEFAULT_LEDS_STARTING_EFFECT);
    leds_starting_color = get_dec_color_from_hex_string(
        "leds", "starting_color", DEFAULT_LEDS_STARTING_COLOR);
    leds_sleeping_effect = get_leds_effect_string("leds", "sleeping_effect",
                                                  DEFAULT_LEDS_SLEEPING_EFFECT);
    leds_sleeping_color = get_dec_color_from_hex_string(
        "leds", "sleeping_color", DEFAULT_LEDS_SLEEPING_COLOR);
    leds_listening_effect = get_leds_effect_string(
        "leds", "listening_effect", DEFAULT_LEDS_LISTENING_EFFECT);
    leds_listening_color = get_dec_color_from_hex_string(
        "leds", "listening_color", DEFAULT_LEDS_LISTENING_COLOR);
    leds_processing_effect = get_leds_effect_string(
        "leds", "processing_effect", DEFAULT_LEDS_PROCESSING_EFFECT);
    leds_processing_color = get_dec_color_from_hex_string(
        "leds", "processing_color", DEFAULT_LEDS_PROCESSING_COLOR);
    leds_saying_effect = get_leds_effect_string("leds", "saying_effect",
                                                DEFAULT_LEDS_SAYING_EFFECT);
    leds_saying_color = get_dec_color_from_hex_string(
        "leds", "saying_color", DEFAULT_LEDS_SAYING_COLOR);
    leds_error_effect = get_leds_effect_string("leds", "error_effect",
                                               DEFAULT_LEDS_ERROR_EFFECT);
    leds_error_color = get_dec_color_from_hex_string("leds", "error_color",
                                                     DEFAULT_LEDS_ERROR_COLOR);
    leds_net_error_effect = get_leds_effect_string(
        "leds", "net_error_effect", DEFAULT_LEDS_NET_ERROR_EFFECT);
    leds_net_error_color = get_dec_color_from_hex_string(
        "leds", "net_error_color", DEFAULT_LEDS_NET_ERROR_COLOR);
    leds_disabled_effect = get_leds_effect_string("leds", "disabled_effect",
                                                  DEFAULT_LEDS_DISABLED_EFFECT);
    leds_disabled_color = get_dec_color_from_hex_string(
        "leds", "diabled_color", DEFAULT_LEDS_DISABLED_COLOR);
  } else {
    leds_type = nullptr;
    leds_path = nullptr;
  }

  // System
  // =========================================================================

  dns_controller_enabled =
      g_key_file_get_boolean(key_file, "system", "dns", nullptr);

  proxy = g_key_file_get_string(key_file, "system", "proxy", nullptr);
  if (!proxy) {
    // use system-wide proxy if available
    proxy = g_strdup(g_getenv("http_proxy"));
  }
  if (proxy) {
    g_print("Proxy enabled: %s\n", proxy);
  }

  error = NULL;
  ssl_strict = g_key_file_get_boolean(key_file, "system", "ssl_strict", &error);
  if (error) {
    ssl_strict = true;
    g_error_free(error);
  }
  if (!ssl_strict) {
    g_warning("SSL strict validation disabled");
  }

  ssl_ca_file =
      g_key_file_get_string(key_file, "system", "ssl_ca_file", nullptr);

  error = NULL;
  cache_dir = g_key_file_get_string(key_file, "system", "cache_dir", &error);
  if (error) {
    g_error_free(error);
    cache_dir = g_strdup_printf("%s/genie", g_get_user_cache_dir());
  }

  if (!g_file_test(cache_dir, G_FILE_TEST_IS_DIR)) {
    if (!g_mkdir_with_parents(cache_dir, 0755)) {
      g_printerr("failed to create cache_dir %s, errno = %d\n", cache_dir,
                 errno);
    }
  }

  // Voice Activity Detection (VAD)
  // =========================================================================

  vad_start_speaking_ms =
      get_bounded_size("vad", "start_speaking_ms",
                       DEFAULT_VAD_START_SPEAKING_MS, VAD_MIN_MS, VAD_MAX_MS);

  vad_done_speaking_ms =
      get_bounded_size("vad", "done_speaking_ms", DEFAULT_VAD_DONE_SPEAKING_MS,
                       VAD_MIN_MS, VAD_MAX_MS);

  vad_input_detected_noise_ms = get_bounded_size(
      "vad", "input_detected_noise_ms", DEFAULT_VAD_INPUT_DETECTED_NOISE_MS,
      VAD_MIN_MS, VAD_MAX_MS);

  vad_listen_timeout_ms = get_bounded_size(
      "vad", "listen_timeout_ms", DEFAULT_VAD_LISTEN_TIMEOUT_MS,
      VAD_LISTEN_TIMEOUT_MIN_MS, VAD_LISTEN_TIMEOUT_MAX_MS);

  // Web UI
  // =========================================================================
  webui_port =
      get_bounded_size("webui", "port", DEFAULT_WEBUI_PORT, 1024, 65535);
}
