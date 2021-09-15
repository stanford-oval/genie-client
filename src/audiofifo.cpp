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

#include <glib.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "audiofifo.hpp"

genie::AudioFIFO::AudioFIFO(App *appInstance) {
  app = appInstance;
  running = reading = false;
}

genie::AudioFIFO::~AudioFIFO() {
  running = false;
  if (fd > 0) {
    close(fd);
  }
  free(ring_buffer.buffer);
}

int genie::AudioFIFO::init() {
  struct stat st;
  if (stat(app->config->audioOutputFIFO, &st) != 0) {
    mkfifo(app->config->audioOutputFIFO, 0666);
  }

  fd = open(app->config->audioOutputFIFO, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    g_printerr("failed to open %s, error %d\n", app->config->audioOutputFIFO, fd);
    return false;
  }

  frame_length = 512;

  int r = fcntl(fd, F_SETPIPE_SZ, frame_length);
  if (r < 0) {
    g_printerr("set pipe size failed, errno = %d\n", errno);
    close(fd);
    return false;
  }

  gint64 pipe_size = fcntl(fd, F_GETPIPE_SZ);
  if (pipe_size == -1) {
    g_printerr("get pipe size failed, errno = %d\n", errno);
    close(fd);
    return false;
  }
  g_message("pipe size: %" G_GINT64_FORMAT "\n", pipe_size);

  pcm = (int16_t *)malloc(frame_length * sizeof(int16_t));
  if (!pcm) {
    g_printerr("failed to allocate memory for audio buffer, errno = %d\n", errno);
    close(fd);
    return false;
  }

  int32_t buffer_size = frame_length * 32;
  int32_t sample_size = sizeof(int16_t);
  void *buffer = calloc(buffer_size, sample_size);
  if (buffer == NULL) {
    g_printerr("failed to allocate memory for ring buffer, errno = %d\n",
               errno);
    close(fd);
    return false;
  }

  ring_buffer_size_t r1 = PaUtil_InitializeRingBuffer(
      &ring_buffer, sample_size, buffer_size, buffer);
  if (r1 == -1) {
    g_printerr("failed to initialize ring buffer\n");
    close(fd);
    return false;
  }

  running = true;
  GError *thread_error = NULL;
  g_thread_try_new("audioFIFOThread", (GThreadFunc)loop, this, &thread_error);
  if (thread_error) {
    g_print("audioFIFOThread Error: g_thread_try_new() %s\n",
            thread_error->message);
    g_error_free(thread_error);
    close(fd);
    return false;
  }

  g_message("Audio playback (output) pipe ready\n");
  return true;
}

void *genie::AudioFIFO::loop(gpointer data) {
  AudioFIFO *obj = reinterpret_cast<AudioFIFO *>(data);

  int32_t sample_size = sizeof(int16_t);
  while (obj->running) {
    int result = read(obj->fd, obj->pcm, obj->frame_length);
    if (result < 0) {
      if (errno != EAGAIN) {
        g_warning("read() returned %d, errno = %d\n", result, errno);
      }
    }
    if (result > 0) {
      // g_debug("readAudioFIFO %d bytes\n", result);
      ring_buffer_size_t wresult = PaUtil_WriteRingBuffer(
          &obj->ring_buffer, obj->pcm, result / sample_size);
      if (wresult < (result / sample_size)) {
        g_warning("readAudioFIFO lost %ld frames\n",
                  (result / sample_size) - wresult);
      }
      obj->reading = true;
    } else if (result == 0) {
      obj->reading = false;
    }
  }

  return NULL;
}

bool genie::AudioFIFO::isReading() {
  return reading;
}
