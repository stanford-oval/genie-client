#define G_LOG_DOMAIN "genie::state::Sleeping"

#include "app.hpp"

namespace genie {
namespace state {

Sleeping::Sleeping(Machine *machine) : State{machine} {}

void Sleeping::enter() {
  g_message("ENTER state Sleeping\n");
  this->app->unduck();
}

} // namespace state
} // namespace genie
