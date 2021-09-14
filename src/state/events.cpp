#include "app.hpp"

#define G_LOG_DOMAIN "genie::state::events"

namespace genie {
namespace state {
namespace events {

// ### Conversation Events ###

InputFrame::InputFrame(AudioFrame *frame) : frame(frame) {}
InputFrame::~InputFrame() {
  // NOTE We do **NOT** free the frame on destruction -- it's handed off to the
  //      STT instance, where it may be queued for later transmission.
}

TextMessage::TextMessage(const gchar *text) : text(g_strdup(text)) {}
TextMessage::~TextMessage() { g_free(text); }

AudioMessage::AudioMessage(const gchar *url) : url(g_strdup(url)) {}
AudioMessage::~AudioMessage() { g_free(url); }

SoundMessage::SoundMessage(Sound_t id) : id(id) {}

AskSpecialMessage::AskSpecialMessage(const gchar *ask) : ask(g_strdup(ask)) {}
AskSpecialMessage::~AskSpecialMessage() { g_free(ask); }

SpotifyCredentials::SpotifyCredentials(const gchar *username,
                                       const gchar *access_token)
    : username(g_strdup(username)), access_token(g_strdup(access_token)) {}

SpotifyCredentials::~SpotifyCredentials() {
  g_free(access_token);
  g_free(username);
}

// ### Physical Button Events ###

AdjustVolume::AdjustVolume(long delta) : delta(delta) {}

} // namespace events
} // namespace state
} // namespace genie
