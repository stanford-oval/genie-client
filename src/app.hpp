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

#ifndef _APP_H
#define _APP_H

#include "config.h"
#include "config.hpp"
#include <glib.h>
#include <memory>
#include <sys/time.h>

#define PROF_PRINT(...)                                                        \
  do {                                                                         \
    struct tm tm;                                                              \
    struct timeval t;                                                          \
    gettimeofday(&t, NULL);                                                    \
    localtime_r(&t.tv_sec, &tm);                                               \
    fprintf(stderr, "[%.2d:%.2d:%.2d.%.6ld] %s (%s:%d): ", tm.tm_hour,         \
            tm.tm_min, tm.tm_sec, t.tv_usec, __func__, __FILE__, __LINE__);    \
    fprintf(stderr, ##__VA_ARGS__);                                            \
  } while (0)

#define PROF_TIME_DIFF(str, start)                                             \
  do {                                                                         \
    struct timeval tEnd;                                                       \
    gettimeofday(&tEnd, NULL);                                                 \
    PROF_PRINT("%s elapsed: %0.3lf ms\n", str, time_diff_ms(start, tEnd));     \
  } while (0)

extern double time_diff(struct timeval x, struct timeval y);
extern double time_diff_ms(struct timeval x, struct timeval y);

namespace genie {

class Config;
class AudioInput;
class AudioFIFO;
class AudioPlayer;
class EVInput;
class Leds;
class Spotifyd;
class STT;
class TTS;
class ConversationClient;
class DNSController;

enum ProcesingEvent_t {
  PROCESSING_BEGIN = 0,
  PROCESSING_START_STT = 1,
  PROCESSING_END_STT = 2,
  PROCESSING_START_GENIE = 3,
  PROCESSING_END_GENIE = 4,
  PROCESSING_START_TTS = 5,
  PROCESSING_END_TTS = 6,
  PROCESSING_FINISH = 7,
};

enum class ActionType {
  WAKE,
  SPEECH_FRAME,
  SPEECH_NOT_DETECTED,
  SPEECH_DONE,
  SPEECH_TIMEOUT,
  DEVICE_KEY,
};

struct AudioFrame {
  int16_t *samples;
  size_t length;

  AudioFrame(size_t len) : samples(new int16_t[len]), length(len) {}
  ~AudioFrame() {
    delete[] samples;
  }

  AudioFrame(const AudioFrame&) = delete;
  AudioFrame& operator=(const AudioFrame&) = delete;

  AudioFrame(AudioFrame&& other) : samples(other.samples), length(other.length) {
    other.samples = nullptr;
    other.length = 0;
  }
};

class App {
public:
  App();
  ~App();
  int exec();

  static gboolean sig_handler(gpointer data);
  static gboolean on_action(gpointer data);

  void track_processing_event(ProcesingEvent_t eventType);
  guint dispatch(ActionType type, gpointer payload);

  GMainLoop *main_loop;
  std::unique_ptr<Config> m_config;
  std::unique_ptr<AudioInput> m_audioInput;
  std::unique_ptr<AudioFIFO> m_audioFIFO;
  std::unique_ptr<AudioPlayer> m_audioPlayer;
  std::unique_ptr<EVInput> m_evInput;
  std::unique_ptr<Leds> m_leds;
  std::unique_ptr<Spotifyd> m_spotifyd;
  std::unique_ptr<STT> m_stt;
  std::unique_ptr<ConversationClient> m_wsClient;
  std::unique_ptr<DNSController> m_dns_controller;

protected:
  void handle(ActionType type, gpointer payload);

private:
  gboolean isProcessing;
  struct timeval tStartProcessing;
  struct timeval tStartSTT;
  struct timeval tEndSTT;
  struct timeval tStartGenie;
  struct timeval tEndGenie;
  struct timeval tStartTTS;
  struct timeval tEndTTS;

  void print_processing_entry(const char *name, double duration_ms,
                              double total_ms);
};

typedef struct {
  App *app;
  ActionType type;
  gpointer payload;
} Action;

} // namespace genie

#endif
