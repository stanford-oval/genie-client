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
#include <glib-unix.h>

#include "app.hpp"
#include "config.hpp"
#include "audioinput.hpp"
#include "audioplayer.hpp"
#include "evinput.hpp"
#include "leds.hpp"
#include "spotifyd.hpp"
#include "stt.hpp"
#include "tts.hpp"
#include "wsclient.hpp"

double time_diff(struct timeval x, struct timeval y)
{
	return (((double)y.tv_sec*1000000 + (double)y.tv_usec) - ((double)x.tv_sec*1000000 + (double)x.tv_usec));
}

genie::App::App()
{
}

genie::App::~App()
{
    g_main_loop_unref(main_loop);
}

int genie::App::exec()
{
    PROF_PRINT("start main loop\n");

    main_loop = g_main_loop_new(NULL, FALSE);

    g_unix_signal_add(SIGINT, sig_handler, main_loop);

    m_config = std::make_unique<Config>();
    m_config->load();

    g_setenv("GST_REGISTRY_UPDATE", "no", true);

    m_audioPlayer = std::make_unique<AudioPlayer>(this);

    m_stt = std::make_unique<STT>(this);
    m_tts = std::make_unique<TTS>(this);

    m_audioInput = std::make_unique<AudioInput>(this);
    m_audioInput->init();

    m_spotifyd = std::make_unique<Spotifyd>(this);
    m_spotifyd->init();

    m_wsClient = std::make_unique<wsClient>(this);
    m_wsClient->init();

    m_evInput = std::make_unique<evInput>(this);
    m_evInput->init();

    m_leds = std::make_unique<Leds>(this);
    m_leds->init();

    g_debug("start main loop\n");
    g_main_loop_run(main_loop);
    g_debug("main loop returned\n");

    return 0;
}

gboolean genie::App::sig_handler(gpointer data)
{
    GMainLoop *loop = reinterpret_cast<GMainLoop *>(data);
    g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}
