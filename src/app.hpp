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

#include "config.h"
#include "config.hpp"
#include "audio.hpp"
#include "state/machine.hpp"
#include <glib.h>
#include <libsoup/soup.h>
#include <memory>
#include <sys/time.h>

#include "autoptrs.hpp"

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

class App {
  friend class state::State;
  friend class state::Sleeping;
  friend class state::Listening;
  friend class state::FollowUp;
  
public:
  App();
  ~App();
  
  // Public Static Methods
  // =========================================================================
  
  static gboolean sig_handler(gpointer data);
  
  // Public Instance Methods
  // =========================================================================
  
  int exec();
  void track_processing_event(ProcesingEvent_t eventType);
  void duck();
  void unduck();
  
  template <typename E>
  guint dispatch(E *event) {
    return state_machine->dispatch(event);
  }

  SoupSession* get_soup_session() {
    return soup_session.get();
  }

  std::unique_ptr<Config> config;

private:
// ===========================================================================

  // Private Instance Members
  // =========================================================================
  
  GMainLoop *main_loop;
  auto_gobject_ptr<SoupSession> soup_session;
  
  // ### Component Instances ###
  
  std::unique_ptr<AudioInput> audio_input;
  std::unique_ptr<AudioPlayer> audio_player;
  std::unique_ptr<ConversationClient> conversation_client;
  std::unique_ptr<DNSController> dns_controller;
  std::unique_ptr<EVInput> ev_input;
  std::unique_ptr<Leds> leds;
  std::unique_ptr<Spotifyd> spotifyd;
  std::unique_ptr<state::Machine> state_machine;
  std::unique_ptr<STT> stt;

  gboolean isProcessing;
  struct timeval tStartProcessing;
  struct timeval tStartSTT;
  struct timeval tEndSTT;
  struct timeval tStartGenie;
  struct timeval tEndGenie;
  struct timeval tStartTTS;
  struct timeval tEndTTS;
  
  // State Variables
  // -------------------------------------------------------------------------
  
  gint64 follow_up_id = -1;
  
  // Private Instance Methods
  // =========================================================================
  
  void print_processing_entry(const char *name, double duration_ms,
                              double total_ms);
  
};

} // namespace genie
