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

#include "audio.hpp"

#include <cstring>

static const char *PROTOCOL_NAME = "protocol:audio";

namespace genie {

namespace conversation {

class CheckAudioResponse
    : private BaseAudioRequest,
      public state::events::Request<state::events::audio::CheckResponse> {
public:
  CheckAudioResponse(Client *client, int64_t req)
      : BaseAudioRequest(client, req) {}

  void resolve(const state::events::audio::CheckResponse &) override;
  void reject(const char *error_code, const char *error_message) override {
    BaseAudioRequest::reject(error_code, error_message);
  };
};

class SimpleAudioResponse : private BaseAudioRequest,
                            public state::events::Request<void> {
public:
  SimpleAudioResponse(Client *client, int64_t req)
      : BaseAudioRequest(client, req) {}

  void resolve() override;
  void reject(const char *error_code, const char *error_message) override {
    BaseAudioRequest::reject(error_code, error_message);
  };
};

} // namespace conversation

} // namespace genie

void genie::conversation::AudioProtocol::connected() {
  const char *caps[] = {nullptr};
  client->request_subprotocol("audio", caps);
}

void genie::conversation::AudioProtocol::handle_message(JsonReader *reader) {
  json_reader_read_member(reader, "req");
  int64_t req = json_reader_get_int_value(reader);
  json_reader_end_member(reader);

  json_reader_read_member(reader, "op");
  const char *op = json_reader_get_string_value(reader);
  json_reader_end_member(reader);

  if (strcmp(op, "check") == 0) {
    handle_check(req, reader);
  } else if (strcmp(op, "prepare") == 0) {
    handle_prepare(req, reader);
  } else if (strcmp(op, "stop") == 0) {
    handle_stop(req, reader);
  } else if (strcmp(op, "play-urls") == 0) {
    handle_play_urls(req, reader);
  } else if (strcmp(op, "set-volume") == 0) {
    handle_set_volume(req, reader);
  } else if (strcmp(op, "set-mute") == 0) {
    handle_set_mute(req, reader);
  } else {
    g_warning("Invalid audio protocol operation %s", op);
    auto *response = new SimpleAudioResponse(client, req);
    response->reject("ENOSYS", "Unknown operation");
    delete response;
  }
}

void genie::conversation::AudioProtocol::handle_check(int64_t req,
                                                      JsonReader *reader) {
  auto request = std::make_unique<CheckAudioResponse>(client, req);

  if (!json_reader_read_member(reader, "spec") ||
      !json_reader_is_object(reader)) {
    json_reader_end_member(reader);
    request->reject("EINVAL",
                    "Missing or invalid player spec in check message");
    return;
  }

  if (!json_reader_read_member(reader, "type")) {
    json_reader_end_member(reader); // end type
    json_reader_end_member(reader); // end spec
    request->reject("EINVAL",
                    "Invalid player spec in check message (missing type)");
    return;
  }

  const char *type = json_reader_get_string_value(reader);
  json_reader_end_member(reader); // end type

  if (strcmp(type, "spotify") == 0) {
    const char *username, *access_token;

    json_reader_read_member(reader, "username");
    username = json_reader_get_string_value(reader);
    json_reader_end_member(reader);

    json_reader_read_member(reader, "accessToken");
    access_token = json_reader_get_string_value(reader);
    json_reader_end_member(reader);

    app->dispatch(new genie::state::events::audio::CheckSpotifyEvent(
        std::move(request), username, access_token));
  } else if (strcmp(type, "url") == 0) {

    // we can always play URLs
    request->resolve(std::make_pair(true, ""));
  } else if (strcmp(type, "custom") == 0) {
    // TODO support custom players by downloading binaries on the fly

    request->resolve(
        std::make_pair(false, "custom binaries are not yet supported"));
  }

  json_reader_end_member(reader); // end spec
}

void genie::conversation::AudioProtocol::handle_prepare(int64_t req,
                                                        JsonReader *reader) {
  auto request = std::make_unique<SimpleAudioResponse>(client, req);

  if (json_reader_read_member(reader, "spec")) {
    if (!json_reader_read_member(reader, "type")) {
      json_reader_end_member(reader); // end type
      json_reader_end_member(reader); // end spec
      request->reject("EINVAL",
                      "Invalid player spec in prepare message (missing type)");
      return;
    }

    const char *type = json_reader_get_string_value(reader);
    json_reader_end_member(reader); // end type

    if (strcmp(type, "spotify") == 0) {
      const char *username, *access_token;

      json_reader_read_member(reader, "username");
      username = json_reader_get_string_value(reader);
      json_reader_end_member(reader);

      json_reader_read_member(reader, "accessToken");
      access_token = json_reader_get_string_value(reader);
      json_reader_end_member(reader);

      app->dispatch(
          new state::events::SpotifyCredentials(username, access_token));
    } else if (strcmp(type, "url") == 0) {
      // nothing to do
    } else if (strcmp(type, "custom") == 0) {
      // TODO support custom players by downloading binaries on the fly

      request->reject("ENOSUP", "custom binaries are not yet supported");
      return;
    }
  }

  app->dispatch(new state::events::audio::PrepareEvent(std::move(request)));
}

void genie::conversation::AudioProtocol::handle_stop(int64_t req,
                                                     JsonReader *reader) {
  auto request = std::make_unique<SimpleAudioResponse>(client, req);

  app->dispatch(new state::events::audio::StopEvent(std::move(request)));
}

void genie::conversation::AudioProtocol::handle_play_urls(int64_t req,
                                                          JsonReader *reader) {
  auto request = std::make_unique<SimpleAudioResponse>(client, req);

  if (!json_reader_read_member(reader, "urls") ||
      !json_reader_is_array(reader)) {
    json_reader_end_member(reader); // end urls
    request->reject("EINVAL", "Missing or invalid urls in play-urls message");
    return;
  }

  std::vector<std::string> urls;
  int n_elements = json_reader_count_elements(reader);
  for (int i = 0; i < n_elements; i++) {
    json_reader_read_element(reader, i);
    urls.emplace_back(json_reader_get_string_value(reader));
    json_reader_end_element(reader);
  }

  json_reader_end_member(reader); // end urls
  app->dispatch(new state::events::audio::PlayURLsEvent(std::move(request),
                                                        std::move(urls)));
}

void genie::conversation::AudioProtocol::handle_set_volume(int64_t req,
                                                           JsonReader *reader) {

  auto request = std::make_unique<SimpleAudioResponse>(client, req);

  json_reader_read_member(reader, "volume");
  int volume = json_reader_get_int_value(reader);
  json_reader_end_member(reader);

  app->dispatch(
      new state::events::audio::SetVolumeEvent(std::move(request), volume));
}

void genie::conversation::AudioProtocol::handle_set_mute(int64_t req,
                                                         JsonReader *reader) {
  auto request = std::make_unique<SimpleAudioResponse>(client, req);

  json_reader_read_member(reader, "mute");
  bool mute = json_reader_get_boolean_value(reader);
  json_reader_end_member(reader);

  app->dispatch(
      new state::events::audio::SetMuteEvent(std::move(request), mute));
}

genie::auto_gobject_ptr<JsonBuilder>
genie::conversation::BaseAudioRequest::make_response() {
  auto_gobject_ptr<JsonBuilder> builder(json_builder_new(), adopt_mode::owned);

  json_builder_begin_object(builder.get());

  json_builder_set_member_name(builder.get(), "type");
  json_builder_add_string_value(builder.get(), PROTOCOL_NAME);

  json_builder_set_member_name(builder.get(), "req");
  json_builder_add_int_value(builder.get(), req);

  return builder;
}

void genie::conversation::BaseAudioRequest::send_response(
    auto_gobject_ptr<JsonBuilder> builder) {
  json_builder_end_object(builder.get()); // end the overall response object
  client->send_json(std::move(builder));
}

void genie::conversation::BaseAudioRequest::reject(const char *error_code,
                                                   const char *error_message) {
  auto builder = make_response();

  json_builder_set_member_name(builder.get(), "error");
  json_builder_begin_object(builder.get());
  if (error_code) {
    json_builder_set_member_name(builder.get(), "code");
    json_builder_add_string_value(builder.get(), error_code);
  }

  json_builder_set_member_name(builder.get(), "message");
  json_builder_add_string_value(builder.get(), error_message ? error_message
                                                             : "unknown error");

  json_builder_end_object(builder.get()); // error

  send_response(std::move(builder));
}

void genie::conversation::CheckAudioResponse::resolve(
    const state::events::audio::CheckResponse &result) {
  auto builder = make_response();

  json_builder_set_member_name(builder.get(), "ok");
  json_builder_add_boolean_value(builder.get(), result.first);

  if (!result.second.empty()) {
    json_builder_set_member_name(builder.get(), "detail");
    json_builder_add_string_value(builder.get(), result.second.c_str());
  }

  send_response(std::move(builder));
}

void genie::conversation::SimpleAudioResponse::resolve() {
  auto builder = make_response();
  send_response(std::move(builder));
}
