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

#include "spotifyd.hpp"
#include <fcntl.h>
#include <glib-unix.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <signal.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <vector>

#define SPOTIFYD_VERSION "0.3.4"

genie::Spotifyd::Spotifyd(App *app) : app(app), child_pid(-1) {}

genie::Spotifyd::~Spotifyd() { close(); }

int genie::Spotifyd::download() {
  const gchar *dl_arch;
  std::string arch;
  struct utsname un;
  if (!uname(&un)) {
    arch = un.machine;
  }
  if (arch == "armv7l") {
    dl_arch = "armhf-";
  } else if (arch == "aarch64") {
    dl_arch = "arm64-";
  } else if (arch == "armv6l") {
    dl_arch = "armv6-";
  } else {
    dl_arch = "";
  }

  gchar *cmd = g_strdup_printf(
      "curl "
      "https://github.com/stanford-oval/spotifyd/releases/download/v%s/"
      "spotifyd-linux-%sslim.tar.gz -L | tar -xvz -C %s",
      SPOTIFYD_VERSION, dl_arch, app->config->cache_dir);
  int rc = system(cmd);
  g_free(cmd);

  return rc;
}

int genie::Spotifyd::check_version() {
  FILE *fp;
  char buf[128];

  gchar *cmd = g_strdup_printf("%s/spotifyd --version", app->config->cache_dir);
  fp = popen(cmd, "r");
  if (fp == NULL) {
    g_free(cmd);
    return true;
  }
  g_free(cmd);

  bool update = false;
  if (fscanf(fp, "spotifyd %127s", buf) == 1) {
    if (strlen(buf) <= 0) {
      g_message("unable to get local spotifyd version, updating...");
      update = true;
    } else {
      if (strcmp(SPOTIFYD_VERSION, buf) != 0) {
        g_message("spotifyd local version %s, need %s, updating...", buf,
                  SPOTIFYD_VERSION);
        update = true;
      }
    }
  }

  if (pclose(fp) == -1) {
    return true;
  }

  return update;
}

int genie::Spotifyd::init() {
  gchar *cmd = g_strdup_printf("killall spotifyd");
  system(cmd);
  g_free(cmd);

  gchar *file_path = g_strdup_printf("%s/spotifyd", app->config->cache_dir);
  if (!g_file_test(file_path, G_FILE_TEST_IS_EXECUTABLE)) {
    download();
  } else {
    if (check_version()) {
      download();
    }
  }
  g_free(file_path);

  if (!username.empty() && !access_token.empty())
    return spawn();
  return true;
}

int genie::Spotifyd::close() {
  if (child_pid > 0) {
    g_debug("kill spotifyd pid %d\n", child_pid);
    return kill(child_pid, SIGINT);
  }
  return true;
}

static void on_pause_sent(SoupSession *session, SoupMessage *msg,
                          gpointer data) {
  GError *error = nullptr;

  guint status_code;
  gchar *status_reason = nullptr;
  g_object_get(msg, "status-code", &status_code, "reason-phrase",
               &status_reason, nullptr);

  g_message("Sent pause request to Spotify, got HTTP %u: %s", status_code,
            status_reason);

  if (status_code >= 400) {
    GBytes *bytes = nullptr;
    g_object_get(msg, "response-body-data", &bytes, nullptr);

    gsize size;
    g_warning("Failed to pause spotifyd through the API: %s",
              (const char *)g_bytes_get_data(bytes, &size));
    g_bytes_unref(bytes);
  }
  g_free(status_reason);
}

void genie::Spotifyd::pause() {
  // compute the device ID of spotifyd
  // it is the sha1 of the device name
  // we pass on the command-line
  std::unique_ptr<GChecksum, fn_deleter<GChecksum, g_checksum_free>> checksum(
      g_checksum_new(G_CHECKSUM_SHA1));
  g_checksum_update(checksum.get(), (const unsigned char *)"genie-cpp",
                    strlen("genie-cpp"));
  const gchar *device_id = g_checksum_get_string(checksum.get());

  g_debug("Sending request to Spotify to pause spotifyd (%s)", device_id);
  std::unique_ptr<SoupURI, fn_deleter<SoupURI, soup_uri_free>> uri(
      soup_uri_new("https://api.spotify.com/v1/me/player/pause"));
  soup_uri_set_query_from_fields(uri.get(), "device_id", device_id, nullptr);

  SoupMessage *msg = soup_message_new_from_uri("PUT", uri.get());
  soup_message_set_request(msg, "application/json", SOUP_MEMORY_STATIC, "", 0);

  SoupMessageHeaders *headers;
  g_object_get(msg, "request-headers", &headers, nullptr);
  soup_message_headers_set_encoding(headers, SOUP_ENCODING_CONTENT_LENGTH);

  char *authorization = g_strdup_printf("Bearer %s", access_token.c_str());
  soup_message_headers_append(headers, "Authorization", authorization);
  g_free(authorization);

  soup_session_queue_message(app->get_soup_session(), msg, on_pause_sent,
                             nullptr);
}

void genie::Spotifyd::child_watch_cb(GPid pid, gint status, gpointer data) {
  Spotifyd *obj = reinterpret_cast<Spotifyd *>(data);
  g_print("spotifyd child %" G_PID_FORMAT " exited with rc %d\n", pid,
          g_spawn_check_exit_status(status, NULL));
  g_spawn_close_pid(pid);
  obj->child_pid = -1;
}

static void child_setup(gpointer user_data) {
  // this function is executed in the child process, between fork() and execve()
  // we have to be very careful here, we can basically only execute syscalls,
  // no memory allocations allowed

  // set this process to receive SIGTERM when the parent (genie-client-cpp) dies
  prctl(PR_SET_PDEATHSIG, SIGTERM);
}

int genie::Spotifyd::spawn() {
  gchar *file_path = g_strdup_printf("%s/spotifyd", app->config->cache_dir);
  const gchar *device_name = "genie-cpp";
  const char *backend = audio_driver_type_to_string(app->config->audio_backend);

  gchar **envp;
  envp = g_get_environ();
  envp = g_environ_setenv(envp, "PULSE_PROP_media.role", "music", TRUE);

  std::vector<const gchar *> argv{
      file_path,       "--no-daemon",
      "--device",      app->config->audio_output_device_music,
      "--device-name", device_name,
      "--device-type", "speaker",
      "--backend",     backend,
      "--username",    username.c_str(),
      "--token",       access_token.c_str(),
      nullptr,
  };

  g_debug("spawn spotifyd");
  for (int i = 0; argv[i]; i++) {
    g_debug("%s", argv[i]);
  }

  GError *gerror = NULL;
  g_spawn_async_with_pipes(NULL, (gchar **)argv.data(), envp,
                           G_SPAWN_DO_NOT_REAP_CHILD, child_setup, NULL,
                           &child_pid, NULL, NULL, NULL, &gerror);
  if (gerror) {
    g_strfreev(envp);
    g_free(file_path);
    g_critical("spawning spotifyd child failed: %s\n", gerror->message);
    g_error_free(gerror);
    return false;
  }

  g_strfreev(envp);
  g_free(file_path);

  g_child_watch_add(child_pid, child_watch_cb, this);
  g_print("spotifyd loaded, pid: %d\n", child_pid);
  return true;
}

bool genie::Spotifyd::set_credentials(const std::string &username,
                                      const std::string &access_token) {
  if (access_token.empty())
    return false;

  if (this->username == username && this->access_token == access_token &&
      child_pid != -1)
    return true;

  this->username = username;
  this->access_token = access_token;
  g_debug("setting spotify username %s access token %s", this->username.c_str(),
          this->access_token.c_str());
  close();
  g_usleep(500);
  return spawn();
}
