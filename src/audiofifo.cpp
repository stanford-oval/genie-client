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

#define G_LOG_DOMAIN "genie-audiofifo"

#include <chrono>
#include <fcntl.h>
#include <glib-unix.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "audiofifo.hpp"

genie::AudioFIFO::AudioFIFO(App *appInstance) {
  app = appInstance;
  running = false;
}

genie::AudioFIFO::~AudioFIFO() {
  running = false;
  if (fd > 0) {
    close(fd);
  }
  free(ring_buffer.buffer);
  free(pcm);
}

int genie::AudioFIFO::init() {
  struct stat st;
  if (stat(app->m_config->audioOutputFIFO, &st) != 0) {
    mkfifo(app->m_config->audioOutputFIFO, 0666);
  }

  fd = open(app->m_config->audioOutputFIFO, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    g_printerr("failed to open %s, error %d\n", app->m_config->audioOutputFIFO,
               fd);
    return false;
  }

  int r = fcntl(fd, F_SETPIPE_SZ, MAX_FRAME_LENGTH__BYTES);
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

  pcm = (int16_t *)malloc(MAX_FRAME_LENGTH__BYTES);
  if (!pcm) {
    g_printerr("failed to allocate memory for audio buffer, errno = %d\n",
               errno);
    close(fd);
    return false;
  }

  void *buffer = calloc(BUFFER_SIZE__FRAMES, sizeof(AudioFrame *));
  if (buffer == NULL) {
    g_printerr("failed to allocate memory for ring buffer, errno = %d\n",
               errno);
    close(fd);
    return false;
  }

  ring_buffer_size_t r1 = PaUtil_InitializeRingBuffer(
      &ring_buffer, sizeof(AudioFrame *), BUFFER_SIZE__FRAMES, buffer);
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

void genie::AudioFIFO::write_to_ring(AudioFrame *frame) {
  ring_buffer_size_t write_size =
      PaUtil_WriteRingBuffer(&ring_buffer, &frame, 1);

  if (write_size < 1) {
    g_warning("ring buffer full -- lost playback audio frame\n");
    delete frame;
  } else {
    // g_message("PUSH AudioFrame length: %d samples\n", frame->length);
  }
}

void *genie::AudioFIFO::loop(gpointer data) {
  AudioFIFO *obj = static_cast<AudioFIFO *>(data);

  while (obj->running) {
    // Read a frame from the pipe into `pcm`.
    //
    // `FRAME_LENGTH__SAMPLES` is in units of _samples_, and `read()` takes a
    // number of _bytes_, so we need to multiple by `BYTES_PER_SAMPLE`.
    //
    ssize_t read_size__bytes = read(obj->fd, obj->pcm, MAX_FRAME_LENGTH__BYTES);
    std::chrono::time_point<std::chrono::steady_clock> captured_at =
      std::chrono::steady_clock::now();

    // Did we encounter an error reading?
    if (read_size__bytes < 0) {
      if (errno != EAGAIN) {
        g_warning("read() returned %d, errno = %d\n", read_size__bytes, errno);
      }
    } else if (read_size__bytes > 0) {
      // Check that we read a whole number of samples
      if (read_size__bytes % BYTES_PER_SAMPLE != 0) {
        g_error("read_size__bytes=%d not divisible by BYTES_PER_SAMPLE=%d\n",
                  read_size__bytes, BYTES_PER_SAMPLE);
      }

      // See how many _samples_ we read
      size_t read_size__samples = read_size__bytes / BYTES_PER_SAMPLE;
      
      AudioFrame *frame = new AudioFrame(read_size__samples, captured_at);
      memcpy(frame->samples, obj->pcm, read_size__samples * BYTES_PER_SAMPLE);
      
      obj->write_to_ring(frame);
    }
    
    usleep(SLEEP__US);
  }

  return NULL;
}
