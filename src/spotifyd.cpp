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

#define SPOTIFYD_VERSION "v0.3.3"

genie::Spotifyd::Spotifyd(App *appInstance) {
    app = appInstance;
    cacheDir = g_strdup("/tmp");
    accessToken = g_strdup("");
    child_pid = -1;

    arch = NULL;
    struct utsname un;
    if (!uname(&un)) {
        arch = g_strdup(un.machine);
    }
}

genie::Spotifyd::~Spotifyd() {
    g_free(arch);
    close();
}

int genie::Spotifyd::download()
{
    gchar *dlArch;
    if (!memcmp(arch, "armv7", 5)) {
        dlArch = (gchar *)"armhf-";
    } else if (!memcmp(arch, "aarch64", 7)) {
        dlArch = (gchar *)"arm64-";
    } else {
        dlArch = (gchar *)"";
    }

    gchar *cmd = g_strdup_printf("curl https://github.com/stanford-oval/spotifyd/releases/download/%s/spotifyd-linux-%sslim.tar.gz -L | tar -xvz -C %s", SPOTIFYD_VERSION, dlArch, cacheDir);
    int rc = system(cmd);
    g_free(cmd);

    return rc;
}

int genie::Spotifyd::init()
{
    if (!arch) {
        return false;
    }

    gchar *filePath = g_strdup_printf("%s/spotifyd", cacheDir);
    int fd = g_open(filePath, O_RDONLY);
    if (fd > 0) {
        GError *error = NULL;
        g_close(fd, &error);
        if (error) {
            g_error_free(error);
        }
        // TODO: check version / update
    } else {
        download();
    }
    g_free(filePath);

    return spawn();
}

int genie::Spotifyd::close()
{
    if (child_pid) {
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
    gchar *deviceName = g_strdup("genie-cpp");

    gchar *backend;
    if (!memcmp(arch, "x86_64", 6)) {
        backend = g_strdup("pulseaudio");
    } else {
        backend = g_strdup("alsa");
    }

    gchar *argv[] = {
        filePath, (gchar *)"--no-daemon", (gchar *)"--device-name", deviceName, (gchar *)"--backend", backend, (gchar *)"--token", accessToken, NULL
    };

    GError *gerror = NULL;
    g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL,
        NULL, &child_pid, NULL, NULL, NULL, &gerror);
    if (gerror) {
        g_error("Spawning child failed: %s\n", gerror->message);
        g_error_free(gerror);
        return false;
    }

    g_free(filePath);
    g_free(deviceName);
    g_free(backend);

    g_child_watch_add(child_pid, child_watch_cb, this);
    g_print("spotifyd loaded, pid: %d\n", child_pid);
    return true;
}

gboolean genie::Spotifyd::setAccessToken(gchar *token)
{
    if (!token) return false;
    accessToken = g_strdup(token);
    close();
    g_usleep(500);
    return spawn();
}
