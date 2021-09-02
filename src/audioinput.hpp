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

#ifndef _AUDIO_H
#define _AUDIO_H

#include <glib.h>
#include <alsa/asoundlib.h>
#include "pv_porcupine.h"
#include "audioplayer.hpp"
#include "stt.hpp"
#include "app.hpp"
#include <webrtc/webrtc_vad.h>

#define AUDIO_INPUT_BUFFER_MAX_FRAMES 32
#define AUDIO_INPUT_VAD_FRAME_LENGTH 480

namespace genie
{
    
enum AudioInputState {
    INPUT_STATE_WAITING,
    INPUT_STATE_STREAMING,
};

class AudioInput
{
public:
    AudioInput(App *appInstance);
    ~AudioInput();
    int init();
    void close();

protected:
    static void *loop(gpointer data);

private:
    gboolean running;
    snd_pcm_t *alsa_handle = NULL;

    void *porcupine_library;
    pv_porcupine_t *porcupine;
    void (*pv_porcupine_delete_func)(pv_porcupine_t *);
    pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *, int32_t *);
    const char *(*pv_status_to_string_func)(pv_status_t);

    int16_t *pcm;
    int32_t frame_length;
    int32_t sample_rate;
    GQueue *pcmQueue;

    VadInst *vadInstance;
    gint vadThreshold;
    AudioPlayer *audioPlayer;
    STT *stt;
    App *app;
    
    static AudioFrame *build_frame(int16_t *samples, gsize length);
};

}

#endif
