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

#include <dlfcn.h>
#include <glib.h>
#include <signal.h>
#include <stdio.h>

#include "wakeword.hpp"

genie::WakeWord::WakeWord(App *app) : app(app) {
  porcupine = nullptr;
  porcupine_library = nullptr;
  pv_porcupine_delete_func = nullptr;
  pv_porcupine_process_func = nullptr;
  pv_status_to_string_func = nullptr;

  char *library_path =
      g_build_filename(app->config->asset_dir, "libpv_porcupine.so", nullptr);

  char *model_path;
  if (app->config->pv_model_path[0] == '/')
    model_path = g_strdup(app->config->pv_model_path);
  else
    model_path = g_build_filename(app->config->asset_dir,
                                  app->config->pv_model_path, nullptr);
  g_message("Loading picovoice model from %s", model_path);

  char *keyword_path;
  if (app->config->pv_keyword_path[0] == '/')
    keyword_path = g_strdup(app->config->pv_keyword_path);
  else
    keyword_path = g_build_filename(app->config->asset_dir,
                                    app->config->pv_keyword_path, nullptr);
  g_message("Loading wakeword from %s", keyword_path);

  const float sensitivity = app->config->pv_sensitivity;

  porcupine_library = dlopen(library_path, RTLD_NOW);
  if (!porcupine_library) {
    g_error("failed to open library %s: %s", library_path, dlerror());
    return;
  }
  g_free(library_path);

  char *error = NULL;

  pv_status_to_string_func = (const char *(*)(pv_status_t))dlsym(
      porcupine_library, "pv_status_to_string");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_status_to_string' with '%s'.\n", error);
    return;
  }

  auto pv_sample_rate_func =
      (decltype(pv_sample_rate) *)dlsym(porcupine_library, "pv_sample_rate");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_sample_rate' with '%s'.\n", error);
    return;
  }

  int32_t sample_rate_signed = pv_sample_rate_func();
  g_assert(sample_rate_signed > 0);
  sample_rate = (size_t)sample_rate_signed;

  auto pv_porcupine_init_func = (decltype(pv_porcupine_init) *)dlsym(
      porcupine_library, "pv_porcupine_init");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_porcupine_init' with '%s'.\n", error);
    return;
  }

  pv_porcupine_delete_func = (decltype(pv_porcupine_delete) *)dlsym(
      porcupine_library, "pv_porcupine_delete");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_porcupine_delete' with '%s'.\n", error);
    return;
  }

  pv_porcupine_process_func = (decltype(pv_porcupine_process) *)dlsym(
      porcupine_library, "pv_porcupine_process");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_porcupine_process' with '%s'.\n", error);
    return;
  }

  auto pv_porcupine_frame_length_func =
      (decltype(pv_porcupine_frame_length) *)dlsym(porcupine_library,
                                                   "pv_porcupine_frame_length");
  if ((error = dlerror()) != NULL) {
    g_error("failed to load 'pv_porcupine_frame_length' with '%s'.\n", error);
    return;
  }

  pv_frame_length = pv_porcupine_frame_length_func();

  porcupine = NULL;
  pv_status_t status = pv_porcupine_init_func(model_path, 1, &keyword_path,
                                              &sensitivity, &porcupine);
  if (status != PV_STATUS_SUCCESS) {
    g_error("'pv_porcupine_init' failed with '%s'\n",
            pv_status_to_string_func(status));
    return;
  }
  g_free(model_path);
  g_free(keyword_path);

  g_print("Initialized wakeword engine, frame length %d, sample rate %zd\n",
          pv_frame_length, sample_rate);
}

genie::WakeWord::~WakeWord() {
  if (porcupine) {
    pv_porcupine_delete_func(porcupine);
  }
  if (porcupine_library) {
    dlclose(porcupine_library);
  }
}

int genie::WakeWord::process(AudioFrame *frame) {
  if (frame->length == 0 || frame->length != (uint32_t)pv_frame_length) {
    return false;
  }

  // Check the frame for the wake-word
  int32_t keyword_index = -1;
  pv_status_t status =
      pv_porcupine_process_func(porcupine, frame->samples, &keyword_index);

  if (status != PV_STATUS_SUCCESS) {
    // Picovoice error!
    g_critical("'pv_porcupine_process' failed with '%s'\n",
               pv_status_to_string_func(status));
    return false;
  }

  if (keyword_index == -1) {
    // wake-word not found
    return false;
  }

  g_message("Detected keyword!\n");
  return true;
}
