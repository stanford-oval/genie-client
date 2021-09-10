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

#define G_LOG_DOMAIN "genie-stt"

#include "stt.hpp"

#include <glib-object.h>
#include <glib-unix.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#include <cstring>
#include <sstream>
#include <string>

static std::string get_ws_url(genie::App *app) {
  const char *nl_url = app->m_config->nlURL;
  g_assert(g_str_has_prefix(nl_url, "http"));

  std::stringstream ws_url;
  ws_url << "ws" << nl_url + 4 << "/en-US/voice/stream";
  return ws_url.str();
}

genie::STT::STT(App *app)
    : m_app(app), m_url(get_ws_url(app)),
      m_soup_session(soup_session_new(), adopt_mode::owned) {
  if (g_str_has_prefix(m_url.c_str(), "wss")) {
    // enable the wss support
    gchar *wss_aliases[] = {(gchar *)"wss", NULL};
    g_object_set(m_soup_session.get(), SOUP_SESSION_HTTPS_ALIASES, wss_aliases,
                 NULL);
  }
}

void genie::STT::complete_success(STTSession *session, const char *text) {
  if (session != m_current_session.get())
    return;
  m_current_session = nullptr;

  m_app->m_audioPlayer.get()->clean_queue();
  m_app->m_wsClient.get()->send_command(text);
}

void genie::STT::complete_error(STTSession *session, int error_code,
                                const char *error_message) {
  if (session != m_current_session.get())
    return;
  m_current_session = nullptr;

  g_warning("STT completed with an error (%d): %s", error_code, error_message);
  m_app->m_audioPlayer.get()->playSound(SOUND_NO_MATCH);
  m_app->m_audioPlayer.get()->resume();
}

void genie::STT::begin_session() {
  if (m_current_session) {
    STTSession::State state = m_current_session->state();
    if (state != STTSession::State::CLOSING &&
        state != STTSession::State::CLOSED) {
      g_warning("Attempting to begin a new STT session while one is already "
                "active (state %d)",
                (int)state);
    }

    m_current_session = nullptr;
  }

  m_current_session = std::make_unique<STTSession>(this, m_url.c_str());
}

void genie::STT::send_done() {
  if (!m_current_session) {
    g_warning("Done event without an active speech to text request");
    return;
  }

  m_current_session->send_done();
}

void genie::STT::send_frame(AudioFrame *frame) {
  if (!m_current_session) {
    g_warning("Sending audio frame without an active speech to text request");
    return;
  }

  m_current_session->send_frame(frame);
}

void genie::STT::record_timing_event(STTSession *session,
                                     genie::STT::Event event) {
  if (session != m_current_session.get())
    return;

  switch (event) {
    case genie::STT::Event::END_STT:
      m_app->track_processing_event(PROCESSING_END_STT);
      break;

    case genie::STT::Event::CONNECT:
      gettimeofday(&tConnect, NULL);
      break;

    case genie::STT::Event::FIRST_FRAME:
      gettimeofday(&tFirstFrame, NULL);
      break;

    case genie::STT::Event::LAST_FRAME:
      gettimeofday(&tLastFrame, NULL);
      break;

    default:
      // TODO
      break;
  }
}

genie::STTSession::STTSession(STT *controller, const char *url)
    : m_controller(controller), m_state(State::INITIAL), m_queue(g_queue_new()),
      m_done(false) {
  g_debug("SST connecting...\n");

  auto_gobject_ptr<SoupMessage> msg(soup_message_new(SOUP_METHOD_GET, url),
                                    adopt_mode::owned);

  soup_session_websocket_connect_async(
      controller->m_soup_session.get(), msg.get(), NULL, NULL, NULL,
      (GAsyncReadyCallback)genie::STTSession::on_connection, this);

  m_state = State::CONNECTING;
}

genie::STTSession::~STTSession() {
  if (m_connection) {
    // remove all signals because the object was deleted
    g_signal_handlers_disconnect_by_data(m_connection.get(), this);
    soup_websocket_connection_close(m_connection.get(),
                                    SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
  }

  while (!g_queue_is_empty(m_queue)) {
    AudioFrame *queued_frame = (AudioFrame *)g_queue_pop_head(m_queue);
    delete queued_frame;
  }
  g_queue_free(m_queue);
}

void genie::STTSession::on_connection(SoupSession *session, GAsyncResult *res,
                                      gpointer data) {
  STTSession *self = static_cast<STTSession *>(data);

  g_debug("STT connected");
  self->m_controller->record_timing_event(self, STT::Event::FIRST_FRAME);

  // PROF_TIME_DIFF(
  //     "STT connect time (connect -> on_connection)",
  //     obj->tConnect
  // );

  SoupWebsocketConnection *conn;
  GError *error = NULL;

  self->m_connection = auto_gobject_ptr<SoupWebsocketConnection>(
      soup_session_websocket_connect_finish(session, res, &error),
      adopt_mode::owned);
  if (error) {
    self->m_controller->complete_error(self, SOUP_WEBSOCKET_CLOSE_ABNORMAL,
                                       error->message);
    g_error_free(error);
    return;
  }
  self->m_state = State::STREAMING;

  soup_websocket_connection_send_text(self->m_connection.get(),
                                      "{ \"ver\": 1 }");
  self->flush_queue();

  g_signal_connect(self->m_connection.get(), "message",
                   G_CALLBACK(genie::STTSession::on_message), self);
  g_signal_connect(self->m_connection.get(), "closed",
                   G_CALLBACK(genie::STTSession::on_close), self);
}

void genie::STTSession::handle_stt_result(const char *text) {
  bool wakeword_found = false;
  if (!strncasecmp(text, "computer,", 9) ||
      !strncasecmp(text, "computer.", 9)) {
    text += 9;
    wakeword_found = true;
  } else if (!strncasecmp(text, "computer", 8)) {
    text += 8;
    wakeword_found = true;
  }

  if (wakeword_found) {
    gchar *dtext = g_strchug(g_strdup(text));

    if (strlen(dtext) > 0) {
      PROF_PRINT("STT mangled: %s\n", dtext);
      m_controller->complete_success(this, dtext);
    } else {
      m_controller->complete_error(this, 400, "wakeword only");
    }

    g_free(dtext);
  } else {
    m_controller->complete_error(this, 400, "wakeword not found");
  }
}

void genie::STTSession::on_message(SoupWebsocketConnection *conn, gint type,
                                   GBytes *message, gpointer data) {
  STTSession *self = static_cast<STTSession *>(data);

  if (type != SOUP_WEBSOCKET_DATA_TEXT) {
    g_warning("Invalid data type %d on STT websocket", type);
    return;
  }
  if (self->m_state != State::STREAMING) {
    g_warning("Received STT final message in invalid state %d",
              (int)self->m_state);
    return;
  }

  self->m_controller->record_timing_event(self, STT::Event::END_STT);
  self->m_state = State::CLOSING;

  gsize sz;
  const gchar *ptr = (const gchar *)g_bytes_get_data(message, &sz);
  g_debug("WS Received data: %s\n", ptr);

  JsonParser *parser = json_parser_new();
  json_parser_load_from_data(parser, ptr, -1, NULL);

  JsonReader *reader = json_reader_new(json_parser_get_root(parser));

  json_reader_read_member(reader, "status");
  int status = json_reader_get_int_value(reader);
  json_reader_end_member(reader);

  if (status == 0) {
    json_reader_read_member(reader, "result");
    const gchar *result = json_reader_get_string_value(reader);
    json_reader_end_member(reader);

    if (strcmp(result, "ok") == 0) {
      json_reader_read_member(reader, "text");
      const gchar *text = json_reader_get_string_value(reader);
      json_reader_end_member(reader);

      // PROF_TIME_DIFF("STT full", obj->app->m_audioInput->tStart);
      PROF_PRINT("STT text: %s\n", text);
      self->handle_stt_result(text);
    }
  } else {
    g_print("STT status %d\n", status);

    json_reader_read_member(reader, "code");
    const char* code = json_reader_get_string_value(reader);
    json_reader_end_member(reader);

    self->m_controller->complete_error(self, status, code);
  }

  g_object_unref(reader);
  g_object_unref(parser);
}

void genie::STTSession::on_close(SoupWebsocketConnection *conn, gpointer data) {
  STTSession *self = static_cast<STTSession *>(data);

  gushort code = soup_websocket_connection_get_close_code(conn);
  self->m_connection = nullptr;

  if (self->m_state != State::CLOSING) {
    g_warning("STT websocket connection closed by server");
    self->m_state = State::CLOSED;

    // note that this function will free self so we cannot access it afterwards
    self->m_controller->complete_error(self, code, nullptr);
  } else {
    g_debug("STT websocket closed (%d)", code);
    self->m_state = State::CLOSED;
  }
}

void genie::STTSession::flush_queue() {
  while (!g_queue_is_empty(m_queue)) {
    AudioFrame *queued_frame = (AudioFrame *)g_queue_pop_head(m_queue);
    dispatch_frame(queued_frame);
  }
}

/**
 * @brief Queue an audio input (speech) frame to be sent to the Speech-To-Text
 * (STT) service.
 *
 * @param frame Audio frame to send.
 */
void genie::STTSession::send_frame(AudioFrame *frame) {
  if (m_done) {
    g_critical("Sending frame after done event");
    return;
  }

  if (is_connection_open()) {
    // If we can send frames (connection is open) then send any queued ones
    // followed by the frame we just received.
    flush_queue();

    dispatch_frame(frame);
  } else {
    // The connection is not open yet, queue the frame to be sent when it does
    // open.
    g_queue_push_tail(m_queue, frame);
  }
}

void genie::STTSession::send_done() {
  if (m_done) {
    g_critical("Duplicate done event");
    return;
  }

  // make an empty frame to indicate the end of speech
  AudioFrame *empty = new AudioFrame(0);
  send_frame(empty);
  m_done = true;
}

void genie::STTSession::dispatch_frame(AudioFrame *frame) {
  soup_websocket_connection_send_binary(m_connection.get(), frame->samples,
                                        frame->length * sizeof(int16_t));
  if (frame->length == 0) {
    m_controller->record_timing_event(this, STT::Event::LAST_FRAME);
  }
  delete frame;
}
