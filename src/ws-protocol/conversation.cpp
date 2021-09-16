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

#include "conversation.hpp"

#include <cstring>

void genie::conversation::ConversationProtocol::handle_message(JsonReader *reader) {
  json_reader_read_member(reader, "type");
  const char *type = json_reader_get_string_value(reader);
  json_reader_end_member(reader);

  // first handle the protocol objects that do not have a sequential ID
  // (because they don't go into the history)
  if (strcmp(type, "id") == 0) {
    handleConversationID(reader);
  } else if (strcmp(type, "askSpecial") == 0) {
    handleAskSpecial(reader);
  } else if (strcmp(type, "error") == 0) {
    handleError(reader);
  } else if (strcmp(type, "ping") == 0) {
    handlePing(reader);
  } else if (strcmp(type, "new-device") == 0) {
    handleNewDevice(reader);
  } else {
    // now handle all the normal messages
    json_reader_read_member(reader, "id");
    gint64 id = json_reader_get_int_value(reader);
    json_reader_end_member(reader);

    g_debug("Handling message id=%" G_GINT64_FORMAT ", setting this->seq", id);
    seq = id;

    if (strcmp(type, "text") == 0) {
      handleText(id, reader);
    } else if (strcmp(type, "sound") == 0) {
      handleSound(id, reader);
    } else if (strcmp(type, "audio") == 0) {
      handleAudio(id, reader);
    } else if (strcmp(type, "command") == 0        // Parrot commands back
               || strcmp(type, "new-program") == 0 // ThingTalk stuff
               || strcmp(type, "rdl") == 0         // External link
               || strcmp(type, "link") == 0        // Internal link (skill conf)
               || strcmp(type, "button") == 0      // Clickable command
               || strcmp(type, "video") == 0 || strcmp(type, "picture") == 0 ||
               strcmp(type, "choice") == 0) {
      g_debug("Ignored message id=%" G_GINT64_FORMAT " type=%s", id, type);
    } else {
      g_warning("Unhandled message id=%" G_GINT64_FORMAT " type=%s\n", id,
                type);
    }
  }
}

void genie::conversation::ConversationProtocol::handleConversationID(JsonReader *reader) {
  json_reader_read_member(reader, "id");
  const gchar *text = json_reader_get_string_value(reader);
  json_reader_end_member(reader);
  g_debug("Received conversation id: %s\n", text);

  // nothing else to do, we already know our conversation ID from the configuration
}

void genie::conversation::ConversationProtocol::handleText(gint64 id, JsonReader *reader) {
  if (id <= last_said_text_id) {
    g_message("Skipping message ID=%" G_GINT64_FORMAT
              ", already said ID=%" G_GINT64_FORMAT "\n",
              id, last_said_text_id);
    return;
  }

  json_reader_read_member(reader, "text");
  const gchar *text = json_reader_get_string_value(reader);
  json_reader_end_member(reader);

  app->dispatch(new state::events::TextMessage(id, text));
  ask_special_text_id = id;
  last_said_text_id = id;
}

void genie::conversation::ConversationProtocol::handleSound(gint64 id, JsonReader *reader) {
  json_reader_read_member(reader, "name");
  const gchar *name = json_reader_get_string_value(reader);
  json_reader_end_member(reader);

  if (strcmp(name, "news-intro") == 0) {
    g_debug("Dispatching sound message id=%" G_GINT64_FORMAT " name=%s\n", id,
            name);
    app->dispatch(new state::events::SoundMessage(Sound_t::NEWS_INTRO));
  } else if (strcmp(name, "alarm-clock-elapsed") == 0) {
    g_debug("Dispatching sound message id=%" G_GINT64_FORMAT " name=%s\n", id,
            name);
    app->dispatch(
        new state::events::SoundMessage(Sound_t::ALARM_CLOCK_ELAPSED));
  } else {
    g_warning("Sound not recognized id=%" G_GINT64_FORMAT " name=%s\n", id,
              name);
  }
}

void genie::conversation::ConversationProtocol::handleAudio(gint64 id, JsonReader *reader) {
  json_reader_read_member(reader, "url");
  const gchar *url = json_reader_get_string_value(reader);
  json_reader_end_member(reader);
  g_debug("Dispatching type=audio id=%" G_GINT64_FORMAT " url=%s\n", id, url);
  app->dispatch(new state::events::AudioMessage(url));
}

void genie::conversation::ConversationProtocol::handleError(JsonReader *reader) {
  json_reader_read_member(reader, "error");
  const gchar *error = json_reader_get_string_value(reader);
  json_reader_end_member(reader);

  g_warning("Handling type=error error=%s\n", error);
}

void genie::conversation::ConversationProtocol::handleAskSpecial(JsonReader *reader) {
  // Agent state -- asking a follow up or not
  json_reader_read_member(reader, "ask");
  const gchar *ask = json_reader_get_string_value(reader);
  json_reader_end_member(reader);
  g_debug("Disptaching type=askSpecial ask=%s for text id=%" G_GINT64_FORMAT,
          ask, ask_special_text_id);
  app->dispatch(new state::events::AskSpecialMessage(ask, ask_special_text_id));
  if (ask_special_text_id != -1) {
    ask_special_text_id = -1;
  }
}

void genie::conversation::ConversationProtocol::handlePing(JsonReader *reader) {
  if (!client->is_connected()) {
    return;
  }

  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());

  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), "pong");

  json_builder_end_object(builder.get());

  client->send_json(builder);
}

void genie::conversation::ConversationProtocol::handleNewDevice(JsonReader *reader) {
  json_reader_read_member(reader, "state");

  json_reader_read_member(reader, "kind");
  const gchar *kind = json_reader_get_string_value(reader);
  if (strcmp(kind, "com.spotify") != 0) {
    g_debug("Ignoring new-device of type %s", kind);
    json_reader_end_member(reader);
    goto out;
  }
  json_reader_end_member(reader);

  {
    const gchar *access_token = nullptr, *username = nullptr;

    if (json_reader_read_member(reader, "accessToken")) {
      access_token = json_reader_get_string_value(reader);
    }
    json_reader_end_member(reader);

    if (json_reader_read_member(reader, "id")) {
      username = json_reader_get_string_value(reader);
    }
    json_reader_end_member(reader);

    if (access_token && username) {
      app->dispatch(
          new state::events::SpotifyCredentials(username, access_token));
    }
  }

out:
  // exit the state reader
  json_reader_end_member(reader);
}
