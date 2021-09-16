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

#include "autoptrs.hpp"
#include "config.hpp"
#include <glib.h>
#include <libsoup/soup.h>
#include <memory>
#include <sys/time.h>

#include "audio.hpp"
#include "state/events.hpp"
#include "state/listening.hpp"
#include "state/processing.hpp"
#include "state/saying.hpp"
#include "state/sleeping.hpp"
#include "state/state.hpp"

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

enum class ProcessingEvent_t {
  BEGIN,
  START_STT,
  END_STT,
  START_GENIE,
  END_GENIE,
  START_TTS,
  END_TTS,
  FINISH,
};

class App {
  friend class state::State;
  friend class state::Sleeping;
  friend class state::Listening;
  friend class state::Processing;
  friend class state::Saying;

public:
  // =========================================================================

  App();
  ~App();

  // Public Static Methods
  // -------------------------------------------------------------------------

  static gboolean sig_handler(gpointer data);

  // Public Instance Members
  // -------------------------------------------------------------------------

  std::unique_ptr<Config> config;

  // Public Instance Methods
  // ---------------------------------------------------------------------------

  int exec();
  void track_processing_event(ProcessingEvent_t eventType);
  void duck();
  void unduck();

  /**
   * @brief Dispatch a state `event`. This method is _thread-safe_.
   *
   * When called it queues handling of the `event` on the main thread via
   * [g_idle_add](https://docs.gtk.org/glib/func.idle_add.html).
   *
   * After the `event` is handled it is deleted.
   */
  template <typename E> guint dispatch(E *event) {
    g_debug("DISPATCH EVENT %s", typeid(E).name());
    DispatchUserData *dispatch_user_data = new DispatchUserData(this, event);
    return g_idle_add(handle<E>, dispatch_user_data);
  }

  SoupSession *get_soup_session() { return soup_session.get(); }

private:
  // =========================================================================

  // Private Structures
  // -------------------------------------------------------------------------

  /**
   * @brief Structure passed as `user_data` through the GLib main loop when
   * dispatching an event.
   *
   * Contains a pointer to the app instance and a pointer to the event being
   * dispatched.
   *
   * After the event is handled the structure is deleted, which in turn
   * deletes the referenced event.
   */
  struct DispatchUserData {
    App *app;
    state::events::Event *event;

    DispatchUserData(App *app, state::events::Event *event)
        : app(app), event(event) {}
    ~DispatchUserData() { delete event; }
  };

  // Private Instance Members
  // -------------------------------------------------------------------------

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
  std::unique_ptr<STT> stt;

  // ### Performance Tracking ###

  bool is_processing;
  struct timeval tStartProcessing;
  struct timeval tStartSTT;
  struct timeval tEndSTT;
  struct timeval tStartGenie;
  struct timeval tEndGenie;
  struct timeval tStartTTS;
  struct timeval tEndTTS;

  // ### State Variables ###

  state::State *current_state;

  // Private Instance Methods
  // =========================================================================

  void print_processing_entry(const char *name, double duration_ms,
                              double total_ms);

  /**
   * @brief GLib main loop callback for dispatching state events.
   *
   * `dispatch()` queues a call to this static method, passing a pointer to a
   * `DispatchUserData` structure that contains pointers to the `App` instance
   * and the dispatched `state::events::Event`.
   *
   * This method calls `state::State::react()` on the `current_state` with
   * the `state::events::Event`, then deletes the event.
   */
  template <typename E> static gboolean handle(gpointer user_data) {
    g_debug("HANDLE EVENT %s", typeid(E).name());
    DispatchUserData *dispatch_user_data =
        static_cast<DispatchUserData *>(user_data);
    E *event = static_cast<E *>(dispatch_user_data->event);
    dispatch_user_data->app->current_state->react(event);
    delete dispatch_user_data;
    return false;
  }

  /**
   * @brief Transit to a new `state::State`.
   *
   * Called in `state::State::react()` implementations to move the app to a
   * the given `new_state`.
   *
   * Responsible for calling `state::State::exit()` on the `current_state` and
   * `state::State::enter()` on the `new_state`.
   */
  template <typename S> void transit(S *new_state) {
    g_message("TRANSIT to %s", S::NAME);
    current_state->exit();
    delete current_state;
    current_state = new_state;
    current_state->enter();
  }
};

} // namespace genie
