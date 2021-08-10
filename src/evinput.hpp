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

#ifndef _EVINPUT_H
#define _EVINPUT_H

#include "app.hpp"

namespace genie
{

struct InputEventSource
{
  GSource source;
  struct libevdev *device;
  GPollFD event_poll_fd;
};

class evInput
{
public:
    evInput(App *appInstance);
    ~evInput();
    int init();

private:
    App *app;
    static gboolean event_prepare(GSource *source, gint *timeout);
    static gboolean event_check(GSource *source);
    static gboolean event_dispatch(GSource *g_source, GSourceFunc callback, gpointer user_data);

    GSourceFuncs event_funcs = {
        event_prepare,
        event_check,
        event_dispatch,
        NULL, NULL, NULL
    };
    GSource *source;
};

}

#endif
