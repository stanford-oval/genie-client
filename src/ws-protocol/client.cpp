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

#include "../audioplayer.hpp"
#include "../spotifyd.hpp"

#include "audio.hpp"
#include "client.hpp"
#include "conversation.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::conversation::Client"

bool genie::conversation::Client::is_connected() {
  if (!m_connection) {
    g_message("GENIE websocket connection is NULL");
    return false;
  }

  SoupWebsocketState wconnState =
      soup_websocket_connection_get_state(m_connection.get());

  if (wconnState != SOUP_WEBSOCKET_STATE_OPEN) {
    g_message("WS connection not open (state %d)", wconnState);
    return false;
  }

  if (!ready) {
    g_debug("Connection is not ready yet to receive messages yet");
  }

  return true;
}

void genie::conversation::Client::send_json(
    auto_gobject_ptr<JsonBuilder> builder) {
  m_outgoing_queue.push_back(builder);
  maybe_flush_queue();
}

void genie::conversation::Client::send_json_now(JsonBuilder *builder) {
  JsonGenerator *gen = json_generator_new();
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(gen, root);
  gchar *str = json_generator_to_data(gen, NULL);

  g_message("Sending: %s", str);
  soup_websocket_connection_send_text(m_connection.get(), str);

  json_node_free(root);
  g_object_unref(gen);
  g_free(str);
}

void genie::conversation::Client::maybe_flush_queue() {
  if (!is_connected())
    return;

  for (const auto &msg : m_outgoing_queue)
    send_json_now(msg.get());
  m_outgoing_queue.clear();
}

void genie::conversation::Client::send_command(const std::string text) {
  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());

  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), "command");

  json_builder_set_member_name(builder.get(), "text");
  json_builder_add_string_value(builder.get(), text.c_str());

  json_builder_end_object(builder.get());

  send_json(builder);

  gettimeofday(&tStart, NULL);

  return;
}

void genie::conversation::Client::send_thingtalk(const char *data) {
  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());

  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), "tt");

  json_builder_set_member_name(builder.get(), "code");
  json_builder_add_string_value(builder.get(), data);

  json_builder_end_object(builder.get());

  send_json(builder);
}

void genie::conversation::Client::request_subprotocol(const char *extension,
                                                      const char *const *caps) {
  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());

  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), "req-subproto");

  json_builder_set_member_name(builder.get(), "proto");
  json_builder_add_string_value(builder.get(), extension);

  json_builder_set_member_name(builder.get(), "caps");
  json_builder_begin_array(builder.get());
  for (int i = 0; caps[i]; i++) {
    json_builder_add_string_value(builder.get(), caps[i]);
  }
  json_builder_end_array(builder.get());

  json_builder_end_object(builder.get());

  send_json(builder);
}

void genie::conversation::Client::on_message(SoupWebsocketConnection *conn,
                                             gint data_type, GBytes *message,
                                             gpointer data) {
  if (data_type != SOUP_WEBSOCKET_DATA_TEXT) {
    g_warning("Invalid message data type: %d", data_type);
    return;
  }

  conversation::Client *obj = static_cast<conversation::Client *>(data);
  gsize sz;
  const gchar *ptr;

  ptr = (const gchar *)g_bytes_get_data(message, &sz);
  g_message("Received message: %s", ptr);

  auto_gobject_ptr<JsonParser> parser(json_parser_new(), adopt_mode::owned);
  json_parser_load_from_data(parser.get(), ptr, -1, NULL);

  auto_gobject_ptr<JsonReader> reader(
      json_reader_new(json_parser_get_root(parser.get())), adopt_mode::owned);

  json_reader_read_member(reader.get(), "type");
  const char *type = json_reader_get_string_value(reader.get());
  json_reader_end_member(reader.get());

  if (g_str_has_prefix(type, "protocol:")) {
    // extension protocol

    const auto &extension = obj->ext_parsers.find(type + strlen("protocol:"));
    if (extension == obj->ext_parsers.end()) {
      g_critical("Unexpected extension protocol message %s", type);
      return;
    }

    extension->second->handle_message(reader.get());
  } else {
    // main protocol
    obj->main_parser->handle_message(reader.get());
  }
}

void genie::conversation::Client::on_close(SoupWebsocketConnection *conn,
                                           gpointer data) {
  conversation::Client *obj = static_cast<conversation::Client *>(data);
  // soup_websocket_connection_close(conn, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);

  const char *close_data = soup_websocket_connection_get_close_data(conn);

  gushort code = soup_websocket_connection_get_close_code(conn);
  g_warning("Genie WebSocket connection closed: %d %s", code, close_data);

  obj->ready = false;
  obj->connect();
}

void genie::conversation::Client::on_connection(SoupSession *session,
                                                GAsyncResult *res,
                                                gpointer data) {
  conversation::Client *self = static_cast<conversation::Client *>(data);
  GError *error = NULL;

  self->m_connection = auto_gobject_ptr<SoupWebsocketConnection>(
      soup_session_websocket_connect_finish(session, res, &error),
      adopt_mode::owned);
  if (error) {
    g_warning("Failed to open websocket connection to Genie: %s",
              error->message);
    g_error_free(error);
    return;
  }
  g_debug("Connected successfully to Genie conversation websocket");

  soup_websocket_connection_set_max_incoming_payload_size(
      self->m_connection.get(), 512000);

  g_signal_connect(self->m_connection.get(), "message",
                   G_CALLBACK(genie::conversation::Client::on_message), data);
  g_signal_connect(self->m_connection.get(), "closed",
                   G_CALLBACK(genie::conversation::Client::on_close), data);
}

void genie::conversation::Client::mark_ready() {
  main_parser->ready();
  for (const auto &it : ext_parsers)
    it.second->ready();

  ready = true;
  maybe_flush_queue();
}

genie::conversation::Client::Client(App *appInstance)
    : app(appInstance), ready(false) {
  accessToken = g_strdup(app->config->genieAccessToken);
  url = g_strdup(app->config->genieURL);
  main_parser.reset(new ConversationProtocol(this));
  ext_parsers.emplace("audio", new AudioProtocol(this));
}

genie::conversation::Client::~Client() {}

int genie::conversation::Client::init() {
  connect();
  return true;
}

void genie::conversation::Client::connect() {
  SoupMessage *msg;

  SoupURI *uri = soup_uri_new(url);
  soup_uri_set_query_from_fields(uri, "skip_history", "1", "sync_devices", "1",
                                 "id", app->config->conversationId, nullptr);

  msg = soup_message_new_from_uri(SOUP_METHOD_GET, uri);
  soup_uri_free(uri);

  if (accessToken) {
    gchar *auth = g_strdup_printf("Bearer %s", accessToken);
    soup_message_headers_append(msg->request_headers, "Authorization", auth);
    g_free(auth);
  }

  soup_session_websocket_connect_async(
      app->get_soup_session(), msg, NULL, NULL, NULL,
      (GAsyncReadyCallback)genie::conversation::Client::on_connection, this);

  return;
}
