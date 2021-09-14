#pragma once

#include "state/events.hpp"
#include "state/state.hpp"

namespace genie {
namespace state {

class Listening : public State {
public:
  Listening(Machine *machine);
  
  void enter();

  void react(events::Wake *);
  void react(events::InputFrame *input_frame);
  void react(events::InputDone *);
  void react(events::InputNotDetected *);
  void react(events::InputTimeout *);
};

} // namespace state
} // namespace genie
