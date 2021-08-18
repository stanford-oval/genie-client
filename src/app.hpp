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

#ifndef _APP_H
#define _APP_H

#include <memory>
#include <glib.h>
#include "config.hpp"
#include "config.h"

#include <sys/time.h>
#define PROF_PRINT(fmt, ...) \
    do { \
        struct tm tm; \
        struct timeval t; \
        gettimeofday(&t, NULL); \
        localtime_r(&t.tv_sec, &tm); \
        fprintf(stderr,"[%.2d:%.2d:%.2d.%.6ld] %s (%s:%d): ", tm.tm_hour, tm.tm_min, tm.tm_sec, t.tv_usec, __func__, __FILE__, __LINE__); \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
    } while (0)

#define PROF_TIME_DIFF(str, start) \
    do { \
        struct timeval tEnd; \
        gettimeofday(&tEnd, NULL); \
        PROF_PRINT("%s elapsed: %.0lfus\n", str, time_diff(start, tEnd)); \
    } while(0)

extern double time_diff(struct timeval x, struct timeval y);

namespace genie {

class Config;
class AudioInput;
class AudioPlayer;
class evInput;
class Leds;
class Spotifyd;
class STT;
class TTS;
class wsClient;

class App
{
public:
    App();
    ~App();
    int exec();

public:
    static gboolean sig_handler(gpointer data);

    GMainLoop *main_loop;
    std::unique_ptr<Config> m_config;
    std::unique_ptr<AudioInput> m_audioInput;
    std::unique_ptr<AudioPlayer> m_audioPlayer;
    std::unique_ptr<evInput> m_evInput;
    std::unique_ptr<Leds> m_leds;
    std::unique_ptr<Spotifyd> m_spotifyd;
    std::unique_ptr<STT> m_stt;
    std::unique_ptr<TTS> m_tts;
    std::unique_ptr<wsClient> m_wsClient;
};

}

#endif
