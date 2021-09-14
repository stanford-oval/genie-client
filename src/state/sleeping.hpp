#pragma once

#include "state/state.hpp"

namespace genie {
namespace state {

class Sleeping : public State {
public:
  Sleeping(Machine *machine);

  void enter() override;
};

} // namespace state
} // namespace genie