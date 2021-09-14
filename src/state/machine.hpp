#pragma once

#include "state/events.hpp"
#include "state/listening.hpp"
#include "state/sleeping.hpp"
#include "state/state.hpp"
#include <typeinfo>

namespace genie {

class App;

namespace state {

// State Machine
// ===========================================================================

struct GMainLoopEvent {
  Machine *machine;
  events::Event *event;
};

class Machine {
public:
  App *app;
  State *current_state;

  Machine(App *app) : app(app){};

  template <typename S> void init() {
    this->current_state = new S(this);
    this->current_state->enter();
  }

  template <typename E> guint dispatch(E *event) {
    g_message("DISPATCH EVENT %s", typeid(E).name());
    GMainLoopEvent *loop_event = new GMainLoopEvent();
    loop_event->machine = this;
    loop_event->event = event;
    return g_idle_add(handle<E>, loop_event);
  }

  template <typename E> static gboolean handle(gpointer user_data) {
    g_message("HANDLE EVENT %s", typeid(E).name());
    GMainLoopEvent *loop_event = static_cast<GMainLoopEvent *>(user_data);
    E *event = static_cast<E *>(loop_event->event);
    loop_event->machine->current_state->react(event);
    delete event;
    return false;
  }

  template <typename S> void transit() {
    g_message("TRANSIT to %s", typeid(S).name());
    this->current_state->exit();
    delete this->current_state;
    this->current_state = new S(this);
    this->current_state->enter();
  }
};

} // namespace state
} // namespace genie
