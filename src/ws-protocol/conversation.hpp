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

#include <cstdint>
#include "client.hpp"

namespace genie {

namespace conversation {

class ConversationProtocol : public ProtocolParser {
public:
  ConversationProtocol(Client *client) : client(client), app(client->app) {}

  void connected() override {}

  void handle_message(JsonReader *msg) override;

private:
  Client *const client;
  App *const app;

  int64_t seq = -1;
  int64_t last_said_text_id = -1;
  int64_t ask_special_text_id = -1;

  // Message handlers
  void handleConversationID(JsonReader *reader);
  void handleText(gint64 id, JsonReader *reader);
  void handleSound(gint64 id, JsonReader *reader);
  void handleAudio(gint64 id, JsonReader *reader);
  void handleError(JsonReader *reader);
  void handlePing(JsonReader *reader);
  void handleAskSpecial(JsonReader *reader);
  void handleNewDevice(JsonReader *reader);
};

} // namespace conversation
} // namespace genie
