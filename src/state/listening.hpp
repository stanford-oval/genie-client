#pragma once

#include "state/events.hpp"
#include "state/state.hpp"

namespace genie {
namespace state {

class Listening : public State {
public:
  static const constexpr char *NAME = "Listening";
  
  Listening(Machine *machine);
  
  void enter() override;

  void react(events::Wake *) override;
  void react(events::InputFrame *input_frame) override;
  void react(events::InputDone *) override;
  void react(events::InputNotDetected *) override;
  void react(events::InputTimeout *) override;
};

} // namespace state
} // namespace genie
