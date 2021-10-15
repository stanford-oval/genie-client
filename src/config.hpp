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

class Config {
public:
  static const size_t VAD_MIN_MS = 100;
  static const size_t VAD_MAX_MS = 5000;
  static const size_t DEFAULT_VAD_START_SPEAKING_MS = 2000;
  static const size_t DEFAULT_VAD_DONE_SPEAKING_MS = 320;
  static const size_t DEFAULT_VAD_INPUT_DETECTED_NOISE_MS = 640;
  static const constexpr char *DEFAULT_AUDIO_OUTPUT_DEVICE = "hw:audiocodec";

  // Picovoice Defaults
  // -------------------------------------------------------------------------

  static const constexpr char *DEFAULT_PV_LIBRARY_PATH =
      "assets/libpv_porcupine.so";
  static const constexpr char *DEFAULT_PV_MODEL_PATH =
      "assets/porcupine_params.pv";
  static const constexpr char *DEFAULT_PV_KEYWORD_PATH = "assets/keyword.ppn";
  static const constexpr float DEFAULT_PV_SENSITIVITY = 0.7f;
  static const constexpr char *DEFAULT_PV_WAKE_WORD_PATTERN =
      "^computer[.,!?]?";

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

  gchar *genieURL;
  gchar *genieAccessToken;
  gchar *conversationId;
  gchar *nlURL;

  // Audio
  // -------------------------------------------------------------------------

  gchar *audioInputDevice;
  gchar *audioSink;
  gchar *audioOutputDeviceMusic;
  gchar *audioOutputDeviceVoice;
  gchar *audioOutputDeviceAlerts;
  gchar *audioOutputFIFO;
  gchar *audioVoice;
  gchar *audio_backend;

  /**
   * @brief Use the audio input as a stereo and convert it to mono
   */
  bool audio_input_stereo2mono;

  // Echo Cancellation
  // -------------------------------------------------------------------------

  bool audio_ec_enabled;

  /**
   * @brief Use the 3rd channel inside audio input stream for the loopback signal
   */
  bool audio_ec_loopback;

  /**
   * @brief The general/main audio output device; used to control volume.
   */
  gchar *audio_output_device;

  // Picovoice (Wake-Word Detection)
  // -------------------------------------------------------------------------

  gchar *pv_library_path;
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

  // System
  // -------------------------------------------------------------------------

  bool dns_controller_enabled;
  gchar *ssl_ca_file;

  // Voice Activity Detection (VAD)
  // -------------------------------------------------------------------------

  size_t vad_start_speaking_ms;
  size_t vad_done_speaking_ms;
  size_t vad_input_detected_noise_ms;

protected:
private:
  int get_leds_effect_string(GKeyFile *key_file, const char *section, const char *key,
                    const gchar *default_value);
  int get_dec_color_from_hex_string(GKeyFile *key_file, const char *section, const char *key,
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
};

} // namespace genie
