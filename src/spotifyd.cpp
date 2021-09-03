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

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-unix.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include "spotifyd.hpp"
#include <sys/utsname.h>
#include <string.h>

#include <vector>

#define SPOTIFYD_VERSION "v0.3.3"

genie::Spotifyd::Spotifyd(App *appInstance) {
    app = appInstance;
    cacheDir = "/tmp";
    child_pid = -1;

    struct utsname un;
    if (!uname(&un)) {
        arch = un.machine;
    }
}

genie::Spotifyd::~Spotifyd() {
    close();
}

int genie::Spotifyd::download()
{
    const gchar *dlArch;
    if (arch == "armv7l") {
        dlArch = "armhf-";
    } else if (arch == "aarch64") {
        dlArch = "arm64-";
    } else {
        dlArch = "";
    }

    gchar *cmd = g_strdup_printf("curl https://github.com/stanford-oval/spotifyd/releases/download/%s/spotifyd-linux-%sslim.tar.gz -L | tar -xvz -C %s", SPOTIFYD_VERSION, dlArch, cacheDir);
    int rc = system(cmd);
    g_free(cmd);

    return rc;
}

int genie::Spotifyd::init()
{
    gchar *filePath = g_strdup_printf("%s/spotifyd", cacheDir);
    if (!g_file_test(filePath, G_FILE_TEST_IS_EXECUTABLE)) {
        download();
    } else {
        // TODO: check version / update
    }
    g_free(filePath);

    if (!m_username.empty() && !m_access_token.empty())
        return spawn();
    return true;
}

int genie::Spotifyd::close()
{
    if (child_pid > 0) {
        g_debug("kill spotifyd pid %d\n", child_pid);
        return kill(child_pid, SIGINT);
    }
    return true;
}

void genie::Spotifyd::child_watch_cb(GPid pid, gint status, gpointer data)
{
    Spotifyd *obj = reinterpret_cast<Spotifyd *>(data);
    g_print("Child %" G_PID_FORMAT " exited with rc %d\n", pid, g_spawn_check_exit_status(status, NULL));
    g_spawn_close_pid(pid);
}

int genie::Spotifyd::spawn()
{
    gchar *filePath = g_strdup_printf("%s/spotifyd", cacheDir);
    const gchar *deviceName = "genie-cpp";
    const gchar *backend;
    if (arch == "x86_64") {
        backend = "pulseaudio";
    } else {
        backend = "alsa";
    }

    std::vector<const gchar*> argv {
        filePath, "--no-daemon",
        "--device-name", deviceName,
        "--device-type", "speaker",
        "--backend", backend,
        "--username", m_username.c_str(),
        "--token", m_access_token.c_str(),
    };
    if (strcmp(backend, "alsa") == 0 && app->m_config->audioOutputDeviceMusic) {
        argv.push_back("--device");
        argv.push_back(app->m_config->audioOutputDeviceMusic);
    }
    argv.push_back(nullptr);

    g_debug("spawn spotifyd");
    for (int i = 0; argv[i]; i++) {
        g_debug("%s", argv[i]);
    }

    GError *gerror = NULL;
    g_spawn_async_with_pipes(NULL, (gchar**) argv.data(), NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL,
        NULL, &child_pid, NULL, NULL, NULL, &gerror);
    if (gerror) {
        g_critical("Spawning spotifyd child failed: %s\n", gerror->message);
        g_error_free(gerror);
        return false;
    }

    g_free(filePath);

    g_child_watch_add(child_pid, child_watch_cb, this);
    g_print("spotifyd loaded, pid: %d\n", child_pid);
    return true;
}

gboolean genie::Spotifyd::set_credentials(const gchar *username, const gchar *token)
{
    if (!token) return false;

    if (m_username == username && m_access_token == token)
        return true;

    m_username = username;
    m_access_token = token;
    g_debug("setting spotify username %s access token %s", m_username.c_str(), m_access_token.c_str());
    close();
    g_usleep(500);
    return spawn();
}
