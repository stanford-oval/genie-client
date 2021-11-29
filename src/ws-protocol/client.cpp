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

#include "../spotifyd.hpp"
#include "../utils/c-style-callback.hpp"
#include "../utils/soup-utils.hpp"
#include "audio/audioplayer.hpp"

#include "audio.hpp"
#include "client.hpp"
#include "conversation.hpp"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::conversation::Client"

using namespace std::literals;

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
  conversation::Client *self = static_cast<conversation::Client *>(data);
  // soup_websocket_connection_close(conn, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);

  const char *close_data = soup_websocket_connection_get_close_data(conn);

  gushort code = soup_websocket_connection_get_close_code(conn);
  g_warning("Genie WebSocket connection closed after %.1LF seconds: %d %s",
            (std::chrono::steady_clock::now() - self->connect_time) / 1.s, code,
            close_data);
  if (self->ping_timeout_id > 0)
    g_source_remove(self->ping_timeout_id);
  self->ping_timeout_id = 0;

  self->ready = false;
  self->retry_connect();
}

gboolean genie::conversation::Client::send_ping(gpointer data) {
  conversation::Client *self = static_cast<conversation::Client *>(data);

  if (!self->ready) {
    self->ping_timeout_id = 0;
    return G_SOURCE_REMOVE;
  }

  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());
  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), "ping");
  json_builder_end_object(builder.get());

  self->send_json_now(builder.get());

  return G_SOURCE_CONTINUE;
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
    self->retry_connect();
    return;
  }
  g_debug("Connected successfully to Genie conversation websocket");
  self->connect_time = std::chrono::steady_clock::now();

  self->ping_timeout_id = g_timeout_add_seconds(30, send_ping, self);

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
    : app(appInstance), ready(false), ping_timeout_id(0) {
  main_parser.reset(new ConversationProtocol(this));
  ext_parsers.emplace("audio", new AudioProtocol(this));
}

genie::conversation::Client::~Client() {
  if (ping_timeout_id > 0)
    g_source_remove(ping_timeout_id);
}

int genie::conversation::Client::init() {
  connect();
  return true;
}

gboolean genie::conversation::Client::retry_connect_timer(gpointer data) {
  conversation::Client *obj = (conversation::Client *)data;
  obj->connect();
  return false;
}

void genie::conversation::Client::retry_connect() {
  g_timeout_add(app->config->retry_interval, retry_connect_timer, this);
}

void genie::conversation::Client::connect_direct(AuthMode auth_mode,
                                                 const char *access_token) {
  SoupURI *uri = soup_uri_new(app->config->genie_url);
  soup_uri_set_query_from_fields(uri, "skip_history", "1", "sync_devices", "1",
                                 "id", app->config->conversation_id, nullptr);

  SoupMessage *msg = soup_message_new_from_uri(SOUP_METHOD_GET, uri);
  soup_uri_free(uri);

  if (auth_mode == AuthMode::BEARER) {
    gchar *auth = g_strdup_printf("Bearer %s", access_token);
    soup_message_headers_append(msg->request_headers, "Authorization", auth);
    g_free(auth);
  } else if (auth_mode == AuthMode::COOKIE) {
    soup_message_headers_append(msg->request_headers, "Cookie", access_token);
  }

  soup_session_websocket_connect_async(
      app->get_soup_session(), msg, NULL, NULL, NULL,
      (GAsyncReadyCallback)genie::conversation::Client::on_connection, this);
  g_object_unref(msg);

  return;
}

void genie::conversation::Client::connect_home_assistant() {
  // in Home Assistant auth mode, we must first get the session token
  SoupURI *home_assistant_url = soup_uri_new(app->config->genie_url);

  soup_uri_set_path(home_assistant_url, "/api/hassio/ingress/session");
  if (strcmp(soup_uri_get_scheme(home_assistant_url), "wss") == 0)
    soup_uri_set_scheme(home_assistant_url, "https");
  else
    soup_uri_set_scheme(home_assistant_url, "http");

  SoupMessage *msg =
      soup_message_new_from_uri(SOUP_METHOD_POST, home_assistant_url);
  soup_uri_free(home_assistant_url);
  soup_message_set_request(msg, nullptr, SOUP_MEMORY_STATIC, "", 0);
  gchar *auth = g_strdup_printf("Bearer %s", app->config->genie_access_token);
  soup_message_headers_append(msg->request_headers, "Authorization", auth);
  g_free(auth);

  send_soup_message(
      app->get_soup_session(), msg,
      [this](SoupSession *session, SoupMessage *msg) {
        gsize size;

        g_message("Sent access token request to Home Assistant, got HTTP %u",
                  msg->status_code);

        if (msg->status_code >= 400) {
          g_warning("Failed to get access token from Home Assistant: %s",
                    msg->response_body->data);
          retry_connect();
          return;
        }

        JsonParser *parser = json_parser_new();
        GError *error = nullptr;
        json_parser_load_from_data(parser, msg->response_body->data,
                                   msg->response_body->length, &error);
        if (error) {
          g_warning(
              "Failed to parse access token reply from Home Assistant: %s",
              error->message);
          g_warning("Response data: %s", msg->response_body->data);
          g_error_free(error);
          retry_connect();
          return;
        }

        JsonReader *reader = json_reader_new(json_parser_get_root(parser));

        json_reader_read_member(reader, "data");

        json_reader_read_member(reader, "session");
        const char *session_token = json_reader_get_string_value(reader);
        gchar *cookie = g_strdup_printf("ingress_session=%s", session_token);
        json_reader_end_member(reader); // session

        json_reader_end_member(reader); // data

        connect_direct(AuthMode::COOKIE, cookie);
        g_free(cookie);

        g_object_unref(parser);
        g_object_unref(reader);
      });
}

void genie::conversation::Client::refresh_oauth2_token() {
  SoupURI *refresh_url = soup_uri_new(app->config->genie_url);

  soup_uri_set_path(refresh_url, "/me/api/oauth2/token");
  if (strcmp(soup_uri_get_scheme(refresh_url), "wss") == 0)
    soup_uri_set_scheme(refresh_url, "https");
  else
    soup_uri_set_scheme(refresh_url, "http");

  SoupMessage *msg = soup_message_new_from_uri(SOUP_METHOD_POST, refresh_url);
  soup_uri_free(refresh_url);

  gchar *body = soup_form_encode("client_id", OAUTH_CLIENT_ID, "client_secret",
                                 OAUTH_CLIENT_SECRET, "grant_type",
                                 "refresh_token", "refresh_token",
                                 app->config->genie_access_token, nullptr);
  soup_message_set_request(msg, "application/x-www-form-urlencoded",
                           SOUP_MEMORY_TAKE, body, strlen(body));
  send_soup_message(
      app->get_soup_session(), msg,
      [this](SoupSession *session, SoupMessage *msg) {
        gsize size;

        g_message("Sent access token refresh to Genie, got HTTP %u",
                  msg->status_code);

        if (msg->status_code >= 400) {
          g_warning("Failed to get access token from Genie: %s",
                    msg->response_body->data);
          retry_connect();
          return;
        }

        JsonParser *parser = json_parser_new();
        GError *error = nullptr;
        json_parser_load_from_data(parser, msg->response_body->data,
                                   msg->response_body->length, &error);
        if (error) {
          g_warning("Failed to parse access token reply from Genie: %s",
                    error->message);
          g_warning("Response data: %s", msg->response_body->data);
          g_error_free(error);
          retry_connect();
          return;
        }

        JsonReader *reader = json_reader_new(json_parser_get_root(parser));

        json_reader_read_member(reader, "access_token");
        temporary_access_token = json_reader_get_string_value(reader);
        json_reader_end_member(reader); // access_token

        connect_direct(AuthMode::BEARER, temporary_access_token.c_str());

        g_object_unref(parser);
        g_object_unref(reader);
      });
}

void genie::conversation::Client::connect_oauth2() {
  if (!app->config->genie_access_token || !*app->config->genie_access_token) {
    g_warning("Missing OAuth token, refusing to connect");
    return;
  }

  if (!temporary_access_token.size()) {
    // if we don't have a token, go straight into refreshing the token
    refresh_oauth2_token();
    return;
  }

  // verify that the token is correct first, because if we connect
  // to WebSocket with a bad token we won't get a good error
  SoupURI *verify_url = soup_uri_new(app->config->genie_url);

  soup_uri_set_path(verify_url, "/me/api/profile");
  if (strcmp(soup_uri_get_scheme(verify_url), "wss") == 0)
    soup_uri_set_scheme(verify_url, "https");
  else
    soup_uri_set_scheme(verify_url, "http");

  SoupMessage *msg = soup_message_new_from_uri(SOUP_METHOD_GET, verify_url);
  soup_uri_free(verify_url);
  gchar *auth = g_strdup_printf("Bearer %s", temporary_access_token.c_str());
  soup_message_headers_append(msg->request_headers, "Authorization", auth);
  g_free(auth);

  send_soup_message(app->get_soup_session(), msg,
                    [this](SoupSession *session, SoupMessage *msg) {
                      if (msg->status_code == 401) {
                        // refresh the token
                        refresh_oauth2_token();
                        return;
                      }

                      if (msg->status_code != 200) {
                        g_warning("Got unexpected HTTP status %d while "
                                  "checking if OAuth token is valid: %s",
                                  msg->status_code, msg->response_body->data);
                        retry_connect();
                        return;
                      }

                      // the token is valid, let's use it
                      connect_direct(AuthMode::BEARER,
                                     temporary_access_token.c_str());
                    });
}

void genie::conversation::Client::connect() {
  if (app->config->auth_mode == AuthMode::HOME_ASSISTANT) {
    connect_home_assistant();
  } else if (app->config->auth_mode == AuthMode::OAUTH2) {
    connect_oauth2();
  } else {
    connect_direct(app->config->auth_mode, app->config->genie_access_token);
  }
}

void genie::conversation::Client::force_reconnect() {
  if (is_connected()) {
    soup_websocket_connection_close(m_connection.get(), 0, nullptr);
    m_connection = nullptr;
  }

  connect();
}
