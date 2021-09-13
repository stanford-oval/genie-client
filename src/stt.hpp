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

#pragma once

#include <glib.h>
#include <libsoup/soup.h>

#include "app.hpp"
#include "audioinput.hpp"
#include "audioplayer.hpp"
#include "wsclient.hpp"
#include "autoptrs.hpp"

namespace genie {

class STT;

class STTSession {
public:
  enum class State {
    INITIAL,
    CONNECTING,
    STREAMING,
    CLOSING,
    CLOSED,
  };

private:
  STT* const m_controller;

  State m_state;
  GQueue* m_queue;
  auto_gobject_ptr<SoupWebsocketConnection> m_connection;
  bool m_done;

  void handle_stt_result(const char *text);

public:
  STTSession(STT* controller, const char *url);
  ~STTSession();

  static void on_connection(SoupSession *session, GAsyncResult *res,
                            gpointer data);
  static void on_message(SoupWebsocketConnection *conn, gint type,
                         GBytes *message, gpointer data);
  static void on_close(SoupWebsocketConnection *conn, gpointer data);

  State state() const {
    return m_state;
  }

  void flush_queue();
  void dispatch_frame(AudioFrame *frame);
  gboolean is_connection_open() {
    return m_state == State::STREAMING;
  }

  void send_frame(AudioFrame *frame);
  void send_done();
};

class STT {
  friend class STTSession;

public:
  STT(App *appInstance);

  void begin_session();
  void send_frame(AudioFrame *frame);
  void send_done();

private:
  enum class Event {
    CONNECT,
    FIRST_FRAME,
    LAST_FRAME,
    END_STT,
  };

  void complete_success(STTSession *session, const char *text);
  void complete_error(STTSession *session, int error_code, const char *error_message);
  void record_timing_event(STTSession *session, Event ev);

  App * const m_app;
  const std::string m_url;
  std::unique_ptr<STTSession> m_current_session;

  struct timeval tConnect;
  struct timeval tFirstFrame;
  struct timeval tLastFrame;
};

} // namespace genie
