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
#include <string>

namespace genie {

class Spotifyd {
public:
  Spotifyd(App *app);
  ~Spotifyd();
  int init();
  int close();
  bool set_credentials(const std::string &username,
                       const std::string &access_token);

protected:
  int spawn();
  int download();
  int check_version();
  static void child_watch_cb(GPid pid, gint status, gpointer data);

private:
  App *app;
  GPid child_pid;
  std::string username;
  std::string access_token;
  std::string arch;
};

} // namespace genie
