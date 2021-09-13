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

#include "app.hpp"
#include <deque>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#include "autoptrs.hpp"

namespace genie {

class ConversationClient {
public:
  ConversationClient(App *appInstance);
  ~ConversationClient();

  int init();
  void send_command(const char *data);
  void send_thingtalk(const char *data);

protected:
  void connect();

private:
  bool is_connected();
  void queue_json(auto_gobject_ptr<JsonBuilder> builder);
  void maybe_flush_queue();
  void send_json_now(JsonBuilder *builder);

  // Socket event handlers
  static void on_connection(SoupSession *session, GAsyncResult *res,
                            gpointer data);
  static void on_message(SoupWebsocketConnection *conn, gint type,
                         GBytes *message, gpointer data);
  static void on_close(SoupWebsocketConnection *conn, gpointer data);

  // Message handlers
  void handleConversationID(JsonReader *reader);
  void handleText(gint64 id, JsonReader *reader);
  void handleSound(gint64 id, JsonReader *reader);
  void handleAudio(gint64 id, JsonReader *reader);
  void handleError(JsonReader *reader);
  void handlePing(JsonReader *reader);
  void handleAskSpecial(JsonReader *reader);
  void handleNewDevice(JsonReader *reader);

  App *app;
  gchar *conversationId;
  gchar *url;
  const gchar *accessToken;
  int seq;
  auto_gobject_ptr<SoupWebsocketConnection> m_connection;
  std::deque<auto_gobject_ptr<JsonBuilder>> m_outgoing_queue;

  struct timeval tStart;
  gboolean tInit;
  gint64 lastSaidTextID;
};

} // namespace genie
