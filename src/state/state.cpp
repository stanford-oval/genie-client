#define G_LOG_DOMAIN "genie::state::State"

#include "app.hpp"
#include "spotifyd.hpp"
#include "audioplayer.hpp"

namespace genie {
namespace state {

State::State(Machine *machine) : machine(machine), app(machine->app) {}

void State::react(events::Event *event) {}

void State::react(events::Wake *) {
  // Normally when we wake we start listening. The exception is the Listen
  // state itself.
  machine->transit<Listening>();
}

void State::react(events::InputFrame *input_frame) {
  g_warning(
      "FIXME received InputFrame when not in Listen state, discarding.\n");
  delete input_frame->frame;
}

void State::react(events::InputDone *) {
  g_warning("FIXME received InputDone when not in Listen state, ignoring.\n");
}

void State::react(events::InputNotDetected *) {
  g_warning("FIXME received InputNotDetected when not in Listen state, "
            "ignoring.\n");
}

void State::react(events::InputTimeout *) {
  g_warning(
      "FIXME received InputTimeout when not in Listen state, ignoring.\n");
}

void State::react(events::TextMessage *text_message) {
  g_message("Received TextMessage, saying text: %s\n", text_message->text);
  app->m_audioPlayer->say(text_message->text);
}

void State::react(events::AudioMessage *audio_message) {
  g_message("Received AudioMessage, playing URL: %s\n", audio_message->url);
  app->m_audioPlayer->stop();
  app->m_audioPlayer->playURI(audio_message->url,
                                      AudioDestination::MUSIC);
}

void State::react(events::SoundMessage *sound_message) {
  g_message("Received SoundMessage, playing sound ID: %d\n",
            sound_message->id);
  app->m_audioPlayer->stop();
  app->m_audioPlayer->playSound(sound_message->id);
}

void State::react(events::AskSpecialMessage *ask_special_message) {
  if (ask_special_message->ask) {
    g_warning("FIXME received AskSpecialMessage with ask=%s, ignoring.",
              ask_special_message->ask);
  } else {
    g_message("Received empty AskSpecialMessage, round done.\n");
  }
}

void State::react(events::SpotifyCredentials *spotify_credentials) {
  app->m_spotifyd->set_credentials(spotify_credentials->username,
                                         spotify_credentials->access_token);
}

void State::react(events::AdjustVolume *adjust_volume) {
  app->m_audioPlayer->adjust_playback_volume(adjust_volume->delta);
}

} // namespace state
} // namespace genie
