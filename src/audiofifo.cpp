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

  int r = fcntl(fd, F_SETPIPE_SZ, FRAME_LENGTH__BYTES * 32);
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

  pcm = (int16_t *)malloc(FRAME_LENGTH__BYTES);
  if (!pcm) {
    g_printerr("failed to allocate memory for audio buffer, errno = %d\n",
               errno);
    close(fd);
    return false;
  }

  zeros = (int16_t *)malloc(FRAME_LENGTH__BYTES);
  if (!pcm) {
    g_printerr("failed to allocate memory for audio buffer, errno = %d\n",
               errno);
    close(fd);
    return false;
  }
  memset(zeros, 0, FRAME_LENGTH__BYTES);

  void *buffer = calloc(BUFFER_SIZE__SAMPLES, BYTES_PER_SAMPLE);
  if (buffer == NULL) {
    g_printerr("failed to allocate memory for ring buffer, errno = %d\n",
               errno);
    close(fd);
    return false;
  }

  ring_buffer_size_t r1 = PaUtil_InitializeRingBuffer(
      &ring_buffer, BYTES_PER_SAMPLE, BUFFER_SIZE__SAMPLES, buffer);
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

void genie::AudioFIFO::write_to_ring(int16_t *samples,
                                     ring_buffer_size_t size__samples) {
  ring_buffer_size_t write_size__samples =
      PaUtil_WriteRingBuffer(&ring_buffer, samples, size__samples);

  if (write_size__samples < size__samples) {
    g_warning(
        "ring buffer full -- lost %ld samples. Given %ld samples, wrote %ld "
        "samples\n",
        size__samples - write_size__samples, size__samples,
        write_size__samples);
  }
}

void *genie::AudioFIFO::loop(gpointer data) {
  AudioFIFO *obj = reinterpret_cast<AudioFIFO *>(data);

  auto last_read_time = std::chrono::steady_clock::now();

  while (obj->running) {
    // Read a frame from the pipe into `pcm`.
    //
    // `FRAME_LENGTH__SAMPLES` is in units of _samples_, and `read()` takes a
    // number of _bytes_, so we need to multiple by `BYTES_PER_SAMPLE`.
    //
    ssize_t read_size__bytes = read(obj->fd, obj->pcm, FRAME_LENGTH__BYTES);

    auto this_read_time = std::chrono::steady_clock::now();
    size_t elapsed__ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             this_read_time - last_read_time)
                             .count();

    size_t elapsed__samples = elapsed__ns / SAMPLE__NS;
    size_t read_size__samples = 0;

    // Did we encounter an error reading?
    if (read_size__bytes < 0) {
      if (errno != EAGAIN) {
        g_warning("read() returned %d, errno = %d\n", read_size__bytes, errno);
      }
    }

    // Did we read any bytes?
    if (read_size__bytes > 0) {
      // Check that we read a whole number of samples
      if (read_size__bytes % BYTES_PER_SAMPLE != 0) {
        g_warning("read_size__bytes=%d not divisible by BYTES_PER_SAMPLE=%d\n",
                  read_size__bytes, BYTES_PER_SAMPLE);
      }

      // See how many _samples_ we read
      read_size__samples = read_size__bytes / BYTES_PER_SAMPLE;

      // Check that we didn't read more samples than how many we _think_ have
      // elapsed. If so, bump up elapsed to the read amount.
      if (read_size__samples > elapsed__samples) {
        g_critical(
            "Read more samples (%d) than apparent elapsed samples (%d),"
            "elapsed time %dns\n",
            read_size__samples, elapsed__samples, elapsed__ns);
        elapsed__samples = read_size__samples;
      }
    }

    size_t leading_zeros__samples = elapsed__samples - read_size__samples;

    obj->write_to_ring(obj->zeros, leading_zeros__samples);

    if (read_size__samples > 0) {
      obj->write_to_ring(obj->pcm, read_size__samples);
    }

    last_read_time = this_read_time;
    usleep(SLEEP__US);
  }

  return NULL;
}
