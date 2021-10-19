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
#include "utils/autoptrs.hpp"
#include <glib.h>
#include <libsoup/soup.h>
#include <memory>
#include <queue>
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
class AudioVolumeController;
class EVInput;
class Leds;
class Spotifyd;
class STT;
class TTS;
class DNSController;
namespace conversation {
class Client;
}

enum class ProcessingEventType {
  START_STT,
  END_STT,
  START_GENIE,
  END_GENIE,
  START_TTS,
  END_TTS,
  DONE,
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

  int exec(int argc, char *argv[]);
  void track_processing_event(ProcessingEventType eventType);

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

  /**
   * Defer the handling of this event until the next state.
   */
  template <typename E> void defer(E *event) {
    if (event != current_event) {
      g_critical("only the currently handled event can be deferred");
      return;
    }
    current_event = nullptr;
    deferred_events.emplace(event);
  }

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
  std::unique_ptr<AudioVolumeController> audio_volume_controller;
  std::unique_ptr<AudioPlayer> audio_player;
  std::unique_ptr<conversation::Client> conversation_client;
  std::unique_ptr<DNSController> dns_controller;
  std::unique_ptr<EVInput> ev_input;
  std::unique_ptr<Leds> leds;
  std::unique_ptr<Spotifyd> spotifyd;
  std::unique_ptr<STT> stt;

  // ### Performance Tracking ###

  bool is_processing;
  struct timeval start_stt;
  struct timeval end_stt;
  struct timeval start_genie;
  struct timeval end_genie;
  struct timeval start_tts;
  struct timeval end_tts;

  // ### State Variables ###

  state::State *current_state;
  state::events::Event *current_event = nullptr;

  /**
   * Event that was deferred by the current state.
   *
   * When an event occurs, a state can decide to defer handling of the event
   * to the next state. Deferred events are replayed upon the next state
   * transition.
   */
  struct DeferredEvent {
    template <typename T>
    DeferredEvent(T *event) : event(event), handler(generic_handler<T>) {}
    DeferredEvent(const DeferredEvent &) = delete;
    DeferredEvent(DeferredEvent &&other) = delete;
    DeferredEvent &operator=(const DeferredEvent &) = delete;
    DeferredEvent &operator=(DeferredEvent &&other) = delete;
    ~DeferredEvent() {
      if (event) {
        g_warning("Deferred event %s was destroyed before it was ever handled",
                  typeid(event).name());
        delete event;
      }
    }

    void dispatch(state::State *state) {
      if (this->event == nullptr) {
        g_critical("Attempting to dispatch a destroyed deferred event");
        return;
      }

      // move the pointer out of here immediately so we can move it down,
      // to ensure the destructor won't accidentally double free
      auto event = this->event;
      this->event = nullptr;
      handler(state, event);
    }

    /**
     * The event that was deferred.
     */
    state::events::Event *event;

  private:
    template <typename T>
    static void generic_handler(state::State *state,
                                state::events::Event *event) {
      state->react(static_cast<T *>(event));
    }

    /**
     * A handler to redispatch the event.
     * This exists to ensure the correct overload is selected
     * based on the type of event.
     */
    void (*handler)(state::State *state, state::events::Event *event);
  };

  std::queue<DeferredEvent> deferred_events;

  // Private Instance Methods
  // =========================================================================

  void init_soup();

  void print_processing_entry(const char *name, double duration_ms,
                              double total_ms);
  void replay_deferred_events();

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
    App *self = dispatch_user_data->app;
    E *event = static_cast<E *>(dispatch_user_data->event);
    // steal the event so we won't free it and the state can defer it
    self->current_event = event;
    dispatch_user_data->event = nullptr;
    self->current_state->react(event);
    delete self->current_event;
    self->current_event = nullptr;
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
    replay_deferred_events();
  }
};

} // namespace genie
