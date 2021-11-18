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

#pragma once

#include <glib.h>

namespace genie {

enum class AuthMode { NONE, BEARER, COOKIE, HOME_ASSISTANT };

class Config {
public:
  static const size_t DEFAULT_WS_RETRY_INTERVAL = 3000;
  static const size_t DEFAULT_CONNECT_TIMEOUT = 5000;
  static const size_t VAD_MIN_MS = 100;
  static const size_t VAD_MAX_MS = 5000;
  static const size_t DEFAULT_VAD_START_SPEAKING_MS = 2000;
  static const size_t DEFAULT_VAD_DONE_SPEAKING_MS = 300;
  static const size_t DEFAULT_VAD_INPUT_DETECTED_NOISE_MS = 300;

  // Max time spent in AudioInput LISTENING state
  static const size_t DEFAULT_VAD_LISTEN_TIMEOUT_MS = 10000;
  static const size_t VAD_LISTEN_TIMEOUT_MIN_MS = 1000;
  static const size_t VAD_LISTEN_TIMEOUT_MAX_MS = 100000;

  static const constexpr char *DEFAULT_PULSE_AUDIO_OUTPUT_DEVICE = "echosink";
  static const constexpr char *DEFAULT_ALSA_AUDIO_OUTPUT_DEVICE = "hw:0,0";
  static const constexpr char *DEFAULT_NLP_URL =
      "https://nlp-staging.almond.stanford.edu";
  static const constexpr char *DEFAULT_LOCALE = "en-US";

  // Hacks Defaults
  // ---------------------------------------------------------------------------

  static const bool DEFAULT_HACKS_WAKE_WORD_VERIFICATION = true;
  static const bool DEFAULT_HACKS_SURPRESS_REPEATED_NOTIFS = false;
  static const constexpr char *DEFAULT_HACKS_DNS_SERVER = "8.8.8.8";

  // Picovoice Defaults
  // -------------------------------------------------------------------------

  static const constexpr char *DEFAULT_PV_LIBRARY_PATH = "libpv_porcupine.so";
  static const constexpr char *DEFAULT_PV_MODEL_PATH = "porcupine_params.pv";
  static const constexpr char *DEFAULT_PV_KEYWORD_PATH = "keyword.ppn";
  static const constexpr float DEFAULT_PV_SENSITIVITY = 0.7f;
  static const constexpr char *DEFAULT_PV_WAKE_WORD_PATTERN =
      "^computers?[.,!?]?";

  // Sound Defaults
  // -------------------------------------------------------------------------

  static const constexpr char *DEFAULT_SOUND_WAKE = "match.oga";
  static const constexpr char *DEFAULT_SOUND_NO_INPUT = "no-match.oga";
  static const constexpr char *DEFAULT_SOUND_TOO_MUCH_INPUT = "no-match.oga";
  static const constexpr char *DEFAULT_SOUND_NEWS_INTRO = "news-intro.oga";
  static const constexpr char *DEFAULT_SOUND_ALARM_CLOCK_ELAPSED =
      "alarm-clock-elapsed.oga";
  static const constexpr char *DEFAULT_SOUND_WORKING = "match.oga";
  static const constexpr char *DEFAULT_SOUND_STT_ERROR = "no-match.oga";

  // Buttons Defaults
  // -------------------------------------------------------------------------
  static const constexpr char *DEFAULT_EVINPUT_DEV = "/dev/input/event0";

  // Leds Defaults
  // -------------------------------------------------------------------------

  static const constexpr char *DEFAULT_LEDS_STARTING_EFFECT = "pulse";
  static const constexpr char *DEFAULT_LEDS_STARTING_COLOR = "0000ff";
  static const constexpr char *DEFAULT_LEDS_SLEEPING_EFFECT = "none";
  static const constexpr char *DEFAULT_LEDS_SLEEPING_COLOR = "000000";
  static const constexpr char *DEFAULT_LEDS_LISTENING_EFFECT = "pulse";
  static const constexpr char *DEFAULT_LEDS_LISTENING_COLOR = "00ff00";
  static const constexpr char *DEFAULT_LEDS_PROCESSING_EFFECT = "circular";
  static const constexpr char *DEFAULT_LEDS_PROCESSING_COLOR = "0000ff";
  static const constexpr char *DEFAULT_LEDS_SAYING_EFFECT = "pulse";
  static const constexpr char *DEFAULT_LEDS_SAYING_COLOR = "8f00ff";
  static const constexpr char *DEFAULT_LEDS_ERROR_EFFECT = "solid";
  static const constexpr char *DEFAULT_LEDS_ERROR_COLOR = "ff0000";
  static const constexpr char *DEFAULT_LEDS_NET_ERROR_EFFECT = "circular";
  static const constexpr char *DEFAULT_LEDS_NET_ERROR_COLOR = "ffa500";
  static const constexpr char *DEFAULT_LEDS_DISABLED_EFFECT = "solid";
  static const constexpr char *DEFAULT_LEDS_DISABLED_COLOR = "ff0000";

  Config();
  ~Config();
  void load();

  // Configuration File Values
  // =========================================================================
  //
  // Loaded from `/opt/genie/config.ini`
  //

  // General
  // -------------------------------------------------------------------------

  gchar *genie_url;
  size_t retry_interval;
  size_t connect_timeout;
  gchar *genie_access_token;
  gchar *conversation_id;
  gchar *nl_url;
  gchar *locale;
  gchar *asset_dir;
  AuthMode auth_mode;

  // Audio
  // -------------------------------------------------------------------------

  gchar *audio_input_device;
  gchar *audio_sink;
  gchar *audio_output_device_music;
  gchar *audio_output_device_voice;
  gchar *audio_output_device_alerts;
  gchar *audio_output_fifo;
  gchar *audio_voice;
  gchar *audio_backend;

  /**
   * @brief Use the audio input as a stereo and convert it to mono
   */
  bool audio_input_stereo2mono;

  // Echo Cancellation
  // -------------------------------------------------------------------------

  bool audio_ec_enabled;

  /**
   * @brief Use the 3rd channel inside audio input stream for the loopback
   * signal
   */
  bool audio_ec_loopback;

  /**
   * @brief The general/main audio output device; used to control volume.
   */
  gchar *audio_output_device;

  // Hacks
  // -------------------------------------------------------------------------
  //
  // Little tricks and tweaks that can help in some specific situations.
  //

  /**
   * @brief Verify presence of wake-word at start of STT response before sending
   * to Genie server.
   *
   * When this setting is `true` Speech-To-Text (STT) responses must match the
   * `pv_wake_word_pattern` to be sent to the server for processing. STT results
   * the do _not_ match the pattern are silently discarded.
   *
   * When this works well it prevents false positives in wake-word detection
   * from sending junk to the server. However -- particularly with "Genie"
   * wake-word variations that we've tested -- the STT produces a wide range of
   * wake-word transcriptions, making it difficult to craft a pattern that
   * matches all of them. This results in a non-negligible amount of false
   * negatives where the client aborts legitimate requests.
   *
   * Setting this flag to `false` can help in situations where you're not
   * concerned about mis-wakes making server requests.
   */
  bool hacks_wake_word_verification;

  /**
   * At the moment (2021-09-30) we have a bug where the server sends
   * notifications _twice_, which is annoying. This switch enabled client-side
   * detection and supression of repeated notifications.
   */
  bool hacks_surpress_repeated_notifs;

  /**
   * DNS server for `genie::DNSController`.
   */
  char *hacks_dns_server;

  // Picovoice (Wake-Word Detection)
  // -------------------------------------------------------------------------

  gchar *pv_model_path;
  gchar *pv_keyword_path;
  float pv_sensitivity;
  gchar *pv_wake_word_pattern;

  // Sounds
  // -------------------------------------------------------------------------

  gchar *sound_wake;
  gchar *sound_no_input;
  gchar *sound_too_much_input;
  gchar *sound_news_intro;
  gchar *sound_alarm_clock_elapsed;
  gchar *sound_working;
  gchar *sound_stt_error;

  // Buttons
  // -------------------------------------------------------------------------
  bool buttons_enabled;
  gchar *evinput_device;

  // Leds
  // -------------------------------------------------------------------------

  bool leds_enabled;
  gchar *leds_type;
  gchar *leds_path;

  gint leds_starting_effect;
  gint leds_starting_color;
  gint leds_sleeping_effect;
  gint leds_sleeping_color;
  gint leds_listening_effect;
  gint leds_listening_color;
  gint leds_processing_effect;
  gint leds_processing_color;
  gint leds_saying_effect;
  gint leds_saying_color;
  gint leds_error_effect;
  gint leds_error_color;
  gint leds_net_error_effect;
  gint leds_net_error_color;
  gint leds_disabled_effect;
  gint leds_disabled_color;

  // System
  // -------------------------------------------------------------------------

  bool dns_controller_enabled;
  gchar *proxy;
  bool ssl_strict;
  gchar *ssl_ca_file;
  gchar *cache_dir;

  // Voice Activity Detection (VAD)
  // -------------------------------------------------------------------------

  size_t vad_start_speaking_ms;
  size_t vad_done_speaking_ms;
  size_t vad_input_detected_noise_ms;
  size_t vad_listen_timeout_ms;

protected:
private:
  int get_leds_effect_string(GKeyFile *key_file, const char *section,
                             const char *key, const gchar *default_value);
  int get_dec_color_from_hex_string(GKeyFile *key_file, const char *section,
                                    const char *key,
                                    const gchar *default_value);
  gchar *get_string(GKeyFile *key_file, const char *section, const char *key,
                    const gchar *default_value);
  size_t get_size(GKeyFile *key_file, const char *section, const char *key,
                  const size_t default_value);
  size_t get_bounded_size(GKeyFile *key_file, const char *section,
                          const char *key, const size_t default_value,
                          const size_t min, const size_t max);
  double get_double(GKeyFile *key_file, const char *section, const char *key,
                    const double default_value);
  double get_bounded_double(GKeyFile *key_file, const char *section,
                            const char *key, const double default_value,
                            const double min, const double max);
  bool get_bool(GKeyFile *key_file, const char *section, const char *key,
                const bool default_value);
};

} // namespace genie
