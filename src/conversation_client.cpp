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

#include <fstream>
#include <glib-object.h>
#include <glib-unix.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <stdlib.h>
#include <string.h>

#include "audioplayer.hpp"
#include "conversation_client.hpp"
#include "spotifyd.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::ConversationClient"

#include <iostream>
#include <ctime>
#include <unistd.h>

using namespace std;

std::string gen_random(const int len) {
    
    std::string tmp_s;
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    srand( (unsigned) time(NULL) * getpid());

    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) 
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    
    
    return tmp_s;
    
}

bool genie::ConversationClient::is_connected() {
  if (!m_connection) {
    g_message("GENIE websocket connection is NULL\n");
    return false;
  }

  SoupWebsocketState wconnState =
      soup_websocket_connection_get_state(m_connection.get());

  if (wconnState != SOUP_WEBSOCKET_STATE_OPEN) {
    g_message("WS connection not open (state %d)\n", wconnState);
    return false;
  }

  return true;
}

void genie::ConversationClient::queue_json(
    auto_gobject_ptr<JsonBuilder> builder) {
  m_outgoing_queue.push_back(builder);
  maybe_flush_queue();
}

void genie::ConversationClient::send_json_now(JsonBuilder *builder) {
  JsonGenerator *gen = json_generator_new();
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(gen, root);
  gchar *str = json_generator_to_data(gen, NULL);

  PROF_PRINT("[SERVER WS] sending: %s\n", str);
  soup_websocket_connection_send_text(m_connection.get(), str);

  json_node_free(root);
  g_object_unref(gen);
  g_free(str);
}

void genie::ConversationClient::maybe_flush_queue() {
  if (!is_connected())
    return;

  for (const auto &msg : m_outgoing_queue)
    send_json_now(msg.get());
  m_outgoing_queue.clear();
}

void genie::ConversationClient::send_command(const std::string text) {
  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());

  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), "command");

  json_builder_set_member_name(builder.get(), "text");
  json_builder_add_string_value(builder.get(), text.c_str());

  json_builder_end_object(builder.get());

  queue_json(builder);

  gettimeofday(&tStart, NULL);

  return;
}

void genie::ConversationClient::send_thingtalk(const char *data) {
  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());

  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), "tt");

  json_builder_set_member_name(builder.get(), "code");
  json_builder_add_string_value(builder.get(), data);

  json_builder_end_object(builder.get());

  queue_json(builder);
}

<<<<<<< HEAD
void genie::ConversationClient::handleConversationID(JsonReader *reader) {
  std::string str = gen_random(12);
  const gchar *text = str.c_str();
=======
void genie::ConversationClient::handleConversationID() {
>>>>>>> feb6d5ada90f19dc5449ef12d16fa3aa19f01bb5

  if (conversationId) {
    g_free(conversationId);
  }
<<<<<<< HEAD
  conversationId = g_strdup(text);
  g_message("Set conversation id: %s", conversationId);
=======
  conversationId = gen_random(12);
  g_message("Set conversation id: %s\n", conversationId);
>>>>>>> feb6d5ada90f19dc5449ef12d16fa3aa19f01bb5
}

void genie::ConversationClient::handleText(gint64 id, JsonReader *reader) {
  if (id <= lastSaidTextID) {
    g_message("Skipping message ID=%" G_GINT64_FORMAT
              ", already said ID=%" G_GINT64_FORMAT "\n",
              id, lastSaidTextID);
    return;
  }

  json_reader_read_member(reader, "text");
  const gchar *text = json_reader_get_string_value(reader);
  json_reader_end_member(reader);

  app->dispatch(new state::events::TextMessage(id, text));
  ask_special_text_id = id;
  lastSaidTextID = id;
}

void genie::ConversationClient::handleSound(gint64 id, JsonReader *reader) {
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

void genie::ConversationClient::handleAudio(gint64 id, JsonReader *reader) {
  json_reader_read_member(reader, "url");
  const gchar *url = json_reader_get_string_value(reader);
  json_reader_end_member(reader);
  g_debug("Dispatching type=audio id=%" G_GINT64_FORMAT " url=%s\n", id, url);
  app->dispatch(new state::events::AudioMessage(url));
}

void genie::ConversationClient::handleError(JsonReader *reader) {
  json_reader_read_member(reader, "error");
  const gchar *error = json_reader_get_string_value(reader);
  json_reader_end_member(reader);

  g_warning("Handling type=error error=%s\n", error);
}

void genie::ConversationClient::handleAskSpecial(JsonReader *reader) {
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

void genie::ConversationClient::handlePing(JsonReader *reader) {
  if (!is_connected()) {
    return;
  }

  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());

  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), "pong");

  json_builder_end_object(builder.get());

  queue_json(builder);
}

void genie::ConversationClient::handleNewDevice(JsonReader *reader) {
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

void genie::ConversationClient::on_message(SoupWebsocketConnection *conn,
                                           gint data_type, GBytes *message,
                                           gpointer data) {
  if (data_type != SOUP_WEBSOCKET_DATA_TEXT) {
    g_warning("Invalid message data type: %d\n", data_type);
    return;
  }

  ConversationClient *obj = static_cast<ConversationClient *>(data);
  gsize sz;
  const gchar *ptr;

  ptr = (const gchar *)g_bytes_get_data(message, &sz);
  g_message("Received message: %s\n", ptr);

  JsonParser *parser = json_parser_new();
  json_parser_load_from_data(parser, ptr, -1, NULL);

  JsonReader *reader = json_reader_new(json_parser_get_root(parser));

  json_reader_read_member(reader, "type");
  const char *type = json_reader_get_string_value(reader);
  json_reader_end_member(reader);

  // first handle the protocol objects that do not have a sequential ID
  // (because they don't go into the history)
  if (strcmp(type, "id") == 0) {
    obj->handleConversationID(reader);
  } else if (strcmp(type, "askSpecial") == 0) {
    obj->handleAskSpecial(reader);
  } else if (strcmp(type, "error") == 0) {
    obj->handleError(reader);
  } else if (strcmp(type, "ping") == 0) {
    obj->handlePing(reader);
  } else if (strcmp(type, "new-device") == 0) {
    obj->handleNewDevice(reader);
  } else {
    // now handle all the normal messages
    json_reader_read_member(reader, "id");
    gint64 id = json_reader_get_int_value(reader);
    json_reader_end_member(reader);

    g_debug("Handling message id=%" G_GINT64_FORMAT ", setting this->seq", id);
    obj->seq = id;

    if (strcmp(type, "text") == 0) {
      obj->handleText(id, reader);
    } else if (strcmp(type, "sound") == 0) {
      obj->handleSound(id, reader);
    } else if (strcmp(type, "audio") == 0) {
      obj->handleAudio(id, reader);
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

  g_object_unref(reader);
  g_object_unref(parser);
}

void genie::ConversationClient::on_close(SoupWebsocketConnection *conn,
                                         gpointer data) {
  ConversationClient *obj = static_cast<ConversationClient *>(data);
  // soup_websocket_connection_close(conn, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);

  const char *close_data = soup_websocket_connection_get_close_data(conn);

  gushort code = soup_websocket_connection_get_close_code(conn);
  g_print("Genie WebSocket connection closed: %d %s\n", code, close_data);

  obj->connect();
}

void genie::ConversationClient::on_connection(SoupSession *session,
                                              GAsyncResult *res,
                                              gpointer data) {
  ConversationClient *self = static_cast<ConversationClient *>(data);
  GError *error = NULL;

  self->m_connection = auto_gobject_ptr<SoupWebsocketConnection>(
      soup_session_websocket_connect_finish(session, res, &error),
      adopt_mode::owned);
  if (error) {
    g_warning("Failed to open websocket connection to Genie: %s\n",
              error->message);
    g_error_free(error);
    return;
  }
  g_debug("Connected successfully to Genie conversation websocket");

  soup_websocket_connection_set_max_incoming_payload_size(
      self->m_connection.get(), 512000);

  g_signal_connect(self->m_connection.get(), "message",
                   G_CALLBACK(genie::ConversationClient::on_message), data);
  g_signal_connect(self->m_connection.get(), "closed",
                   G_CALLBACK(genie::ConversationClient::on_close), data);

  self->maybe_flush_queue();
}

genie::ConversationClient::ConversationClient(App *appInstance) {
  app = appInstance;
  conversationId = NULL;
  accessToken = g_strdup(app->config->genieAccessToken);
  url = g_strdup(app->config->genieURL);

  lastSaidTextID = -1;
  ask_special_text_id = -1;
}

genie::ConversationClient::~ConversationClient() {}

int genie::ConversationClient::init() {
  connect();
  return true;
}

void genie::ConversationClient::connect() {
  SoupMessage *msg;

  SoupURI *uri = soup_uri_new(url);
  if (app->config->conversationId) {
    soup_uri_set_query_from_fields(uri, "skip_history", "1", "sync_devices",
                                   "1", "id", app->config->conversationId,
                                   nullptr);
  } else {
    soup_uri_set_query_from_fields(uri, "skip_history", "1", "sync_devices",
                                   "1", nullptr);
  }

  msg = soup_message_new_from_uri(SOUP_METHOD_GET, uri);
  soup_uri_free(uri);

  if (accessToken) {
    gchar *auth = g_strdup_printf("Bearer %s", accessToken);
    soup_message_headers_append(msg->request_headers, "Authorization", auth);
    g_free(auth);
  }

  soup_session_websocket_connect_async(
      app->get_soup_session(), msg, NULL, NULL, NULL,
      (GAsyncReadyCallback)genie::ConversationClient::on_connection, this);

  return;
}
