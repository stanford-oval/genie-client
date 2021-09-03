// -*- mode: cpp; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "leds.hpp"
#include <fcntl.h>
#include <glib-unix.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

genie::Leds::Leds(App *appInstance) { app = appInstance; }

genie::Leds::~Leds() {}

int genie::Leds::init() { return true; }

int genie::Leds::setEffect(int mode) {
  if (mode < 0 || mode > 7)
    return false;

  char buffer[8];
  memset(buffer, 0, sizeof(buffer));
  snprintf(buffer, sizeof(buffer) - 1, "%d", mode);

  int fd = g_open("...", O_WRONLY);
  if (fd > 0) {
    write(fd, buffer, sizeof(buffer));
  }

  GError *error = NULL;
  g_close(fd, &error);
  if (error) {
    g_error_free(error);
  }

  return true;
}
