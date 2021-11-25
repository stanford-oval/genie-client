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

#pragma once

#include "../app.hpp"
#include "../utils/autoptrs.hpp"
#include <chrono>
#include <deque>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>
#include <string>
#include <unordered_map>

namespace genie {

namespace conversation {

class ProtocolParser {
public:
  virtual ~ProtocolParser() = default;

  virtual void connected() = 0;

  virtual void ready() = 0;

  virtual void handle_message(JsonReader *reader) = 0;
};

class Client {
  friend class ConversationProtocol;
  friend class AudioProtocol;
  friend class BaseAudioRequest;

public:
  Client(App *appInstance);
  ~Client();

  int init();
  void force_reconnect();
  void send_command(const std::string text);
  void send_thingtalk(const char *data);
  void request_subprotocol(const char *extension, const char *const *caps);

  void set_temporary_access_token(const char *token) {
    temporary_access_token = token;
  }

protected:
  void send_json(auto_gobject_ptr<JsonBuilder> builder);
  bool is_connected();
  const char *conversation_id() { return app->config->conversation_id; }
  void mark_ready();

  App *const app;

private:
  void connect();
  void connect_home_assistant();
  void connect_oauth2();
  void connect_direct(AuthMode auth_mode, const char *access_token);
  void refresh_oauth2_token();
  std::string temporary_access_token;

  static gboolean retry_connect_timer(gpointer data);
  void retry_connect();
  void maybe_flush_queue();
  void send_json_now(JsonBuilder *builder);

  // Socket event handlers
  static void on_connection(SoupSession *session, GAsyncResult *res,
                            gpointer data);
  static void on_message(SoupWebsocketConnection *conn, gint type,
                         GBytes *message, gpointer data);
  static void on_close(SoupWebsocketConnection *conn, gpointer data);
  static gboolean send_ping(gpointer data);

  auto_gobject_ptr<SoupWebsocketConnection> m_connection;
  std::deque<auto_gobject_ptr<JsonBuilder>> m_outgoing_queue;
  bool ready;
  std::chrono::steady_clock::time_point connect_time;
  unsigned int ping_timeout_id;

  std::unique_ptr<ProtocolParser> main_parser;
  std::unordered_map<std::string, std::unique_ptr<ProtocolParser>> ext_parsers;

  struct timeval tStart;
};

} // namespace conversation

} // namespace genie
