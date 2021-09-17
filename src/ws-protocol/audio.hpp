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

#include "client.hpp"
#include "state/events.hpp"
#include <cstdint>

namespace genie {

namespace conversation {

class BaseAudioRequest {
public:
  virtual ~BaseAudioRequest();

protected:
  BaseAudioRequest(Client *client, int64_t req) : client(client), req(req) {}

  auto_gobject_ptr<JsonBuilder> make_response();
  void send_response(auto_gobject_ptr<JsonBuilder> builder);
  void reject(const char *error_code, const char *error_message);

private:
  bool handled = false;
  Client *const client;
  const int64_t req;
};

class AudioProtocol : public ProtocolParser {
public:
  AudioProtocol(Client *client) : client(client), app(client->app) {}

  void connected() override {}
  void ready() override;

  void handle_message(JsonReader *reader) override;

private:
  Client *client;
  App *app;

  void handle_check(int64_t req, JsonReader *reader);
  void handle_prepare(int64_t req, JsonReader *reader);
  void handle_stop(int64_t req, JsonReader *reader);
  void handle_play_urls(int64_t req, JsonReader *reader);
  void handle_set_volume(int64_t req, JsonReader *reader);
  void handle_set_mute(int64_t req, JsonReader *reader);
};

} // namespace conversation

} // namespace genie
