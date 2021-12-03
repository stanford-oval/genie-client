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

#include "utils/autoptrs.hpp"
#include <libsoup/soup.h>
#include <mustache.hpp>
#include <string>

namespace genie {

class App;

class WebServer {
public:
  WebServer(App *app);

private:
  App *app;
  auto_gobject_ptr<SoupServer> server;
  std::string csrf_token;
  kainjow::mustache::mustache page_layout;

  void log_request(SoupMessage *msg, const char *path, int status);

  void send_html(SoupMessage *msg, int status, const char *page_title,
                 const char *page_body);
  void send_error(SoupMessage *msg, int status, const char *error);

  enum class AllowedMethod { NONE = 0, GET = 1, POST = 2 };
  AllowedMethod check_method(SoupMessage *msg, const char *path,
                             int allowed_methods);

  void handle_asset(SoupMessage *msg, const char *path);

  void handle_index(SoupMessage *msg);
  void handle_index_post(SoupMessage *msg);
  void handle_index_get(SoupMessage *msg);
  void handle_oauth_redirect(SoupMessage *msg, GHashTable *query);
  void handle_404(SoupMessage *msg, const char *path);
  void handle_405(SoupMessage *msg, const char *path);
};

} // namespace genie
