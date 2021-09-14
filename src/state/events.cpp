#define G_LOG_DOMAIN "genie::state::events"

#include "app.hpp"

namespace genie {
namespace state {
namespace events {

// ### Conversation Events ###

InputFrame::InputFrame(AudioFrame *frame) : frame(frame) {}
InputFrame::~InputFrame() {
  // NOTE We do **NOT** free the frame on destruction -- it's handed off to the
  //      STT instance, where it may be queued for later transmission.
}

TextMessage::TextMessage(gchar *text) : text(g_strdup(text)) {}
TextMessage::~TextMessage() { g_free(text); }

AudioMessage::AudioMessage(gchar *url) : url(g_strdup(url)) {}
AudioMessage::~AudioMessage() { g_free(url); }

SoundMessage::SoundMessage(Sound_t id) : id(id) {}

AskSpecialMessage::AskSpecialMessage(gchar *ask) : ask(g_strdup(ask)) {}
AskSpecialMessage::~AskSpecialMessage() { g_free(ask); }

SpotifyCredentials::SpotifyCredentials(gchar *access_token, gchar *username)
    : access_token(g_strdup(access_token)),
      username(g_strdup(username)) {}

SpotifyCredentials::~SpotifyCredentials() {
  g_free(access_token);
  g_free(username);
}

// ### Physical Button Events ###

AdjustVolume::AdjustVolume(long delta) : delta(delta) {}

} // namespace events
} // namespace state
} // namespace genie
