#pragma once

#include "state/events.hpp"

namespace genie {

class App;

namespace state {

class Machine;

class State {
public:
  Machine *machine;
  App *app;

  State(Machine *machine);
  virtual ~State() = default;

  virtual void enter() {};
  virtual void exit() {};

  virtual void react(events::Event *event);
  virtual void react(events::Wake *);
  virtual void react(events::InputFrame *input_frame);
  virtual void react(events::InputDone *);
  virtual void react(events::InputNotDetected *);
  virtual void react(events::InputTimeout *);
  virtual void react(events::TextMessage *text_message);
  virtual void react(events::AudioMessage *audio_message);
  virtual void react(events::SoundMessage *sound_message);
  virtual void react(events::AskSpecialMessage *ask_special_message);
  virtual void react(events::SpotifyCredentials *spotify_credentials);
  virtual void react(events::AdjustVolume *adjust_volume);
};

} // namespace state
} // namespace genie
