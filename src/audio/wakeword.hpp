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
#include "pv_porcupine.h"

namespace genie {

class WakeWord {
public:
  WakeWord(App *app);
  ~WakeWord();
  int init();
  int process(AudioFrame *frame);

  int32_t pv_frame_length;
  size_t sample_rate;

private:
  // initialized once and never overwritten
  App *const app;

  void *porcupine_library;
  pv_porcupine_t *porcupine;
  void (*pv_porcupine_delete_func)(pv_porcupine_t *);
  pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *,
                                           int32_t *);
  const char *(*pv_status_to_string_func)(pv_status_t);
};

} // namespace genie
