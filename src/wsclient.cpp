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

#include <stdlib.h>
#include <glib.h>
#include <glib-unix.h>
#include <glib-object.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include "wsclient.hpp"
#include "tts.hpp"

void genie::wsClient::sendCommand(gchar *data)
{
    
    if (!wconn || soup_websocket_connection_get_state(wconn) != SOUP_WEBSOCKET_STATE_OPEN) {
        g_print("WS connection not open\n");
        return;
    }

    JsonBuilder *builder = json_builder_new();

    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, "command");

    json_builder_set_member_name(builder, "text");
    json_builder_add_string_value(builder, data);

    /*json_builder_set_member_name(builder, "id");
    seq++;
    json_builder_add_int_value(builder, seq);*/

    json_builder_end_object(builder);

    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);
    gchar *str = json_generator_to_data(gen, NULL);

    PROF_PRINT("WS sendCommand: %s\n", str);
    gettimeofday(&tStart, NULL);
    app->track_processing_event(PROCESSING_START_GENIE);
    tInit = true;

    g_debug("WS sendCommand: %s\n", str);
    soup_websocket_connection_send_text(wconn, str);

    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);
    g_free(str);

    return;
}

void genie::wsClient::sendThingtalk(gchar *data)
{
    if (soup_websocket_connection_get_state(wconn) != SOUP_WEBSOCKET_STATE_OPEN) {
        g_print("WS connection not open\n");
        return;
    }

    JsonBuilder *builder = json_builder_new();

    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "type");
    json_builder_add_string_value(builder, "tt");

    json_builder_set_member_name(builder, "code");
    json_builder_add_string_value(builder, data);

    json_builder_set_member_name(builder, "id");
    seq++;
    json_builder_add_int_value(builder, seq);

    json_builder_end_object(builder);

    JsonGenerator *gen = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(gen, root);
    gchar *str = json_generator_to_data(gen, NULL);

    g_debug("WS sendThingtalk: %s\n", str);
    soup_websocket_connection_send_text(wconn, str);

    json_node_free(root);
    g_object_unref(gen);
    g_object_unref(builder);
    g_free(str);

    return;
}

void genie::wsClient::on_message(SoupWebsocketConnection *conn, gint type, GBytes *message, gpointer data)
{
    if (type == SOUP_WEBSOCKET_DATA_TEXT) {
        wsClient *obj = reinterpret_cast<wsClient *>(data);
        gsize sz;
        const gchar *ptr;

        ptr = (const gchar *)g_bytes_get_data(message, &sz);
        g_debug("WS Received data: %s\n", ptr);

        JsonParser *parser = json_parser_new();
        json_parser_load_from_data(parser, ptr, -1, NULL);

        JsonReader *reader = json_reader_new(json_parser_get_root(parser));

        json_reader_read_member(reader, "type");
        const char *type = json_reader_get_string_value(reader);
        json_reader_end_member(reader);

        if (!memcmp(type, "id", 2)) {
            json_reader_read_member(reader, "id");
            const char *text = json_reader_get_string_value(reader);
            json_reader_end_member(reader);
            if (obj->conversationId) {
                g_free(obj->conversationId);
            }
            obj->conversationId = g_strdup(text);
            g_print("Conversation id: %s\n", obj->conversationId);
            obj->acceptStream = true;
        } else {
            json_reader_read_member(reader, "id");
            obj->seq = json_reader_get_int_value(reader);
            json_reader_end_member(reader);
        }

        if (obj->acceptStream && memcmp(type, "id", 2)) {
            if (!memcmp(type, "text", 4)) {
                json_reader_read_member(reader, "text");
                const gchar *text = json_reader_get_string_value(reader);
                json_reader_end_member(reader);

                // PROF_PRINT("Start speak text: %s\n", text);
                if (obj->tInit) {
                    obj->app->track_processing_event(PROCESSING_END_GENIE);
                    PROF_TIME_DIFF("command response", obj->tStart);
                    obj->tInit = false;
                }

                obj->app->m_audioPlayer->say(g_strdup(text));
            } else if (!memcmp(type, "sound", 5)) {
                json_reader_read_member(reader, "name");
                const gchar *text = json_reader_get_string_value(reader);
                json_reader_end_member(reader);
                if (!memcmp(text, "news-intro", 10)) {
                    g_print("Start sound: %s\n", text);
                    obj->app->m_audioPlayer->playSound(SOUND_NEWS_INTRO, true);
                } else {
                    g_print("Sound not recognized: %s\n", text);
                }
            } else if (!memcmp(type, "audio", 5)) {
                json_reader_read_member(reader, "url");
                const gchar *text = json_reader_get_string_value(reader);
                json_reader_end_member(reader);
                g_print("Start play location: %s\n", text);
                obj->app->m_audioPlayer->playLocation((gchar *)text);
            } else if (!memcmp(type, "error", 4)) {
                json_reader_read_member(reader, "error");
                const gchar *text = json_reader_get_string_value(reader);
                json_reader_end_member(reader);
                g_print("WS Remote error: %s\n", text);
            } else if (!memcmp(type, "command", 7)) {
                json_reader_read_member(reader, "command");
                const gchar *text = json_reader_get_string_value(reader);
                json_reader_end_member(reader);
                g_print("WS Command: %s\n", text);
            } else if (!memcmp(type, "askSpecial", 10)) {
                json_reader_read_member(reader, "ask");
                const gchar *text = json_reader_get_string_value(reader);
                json_reader_end_member(reader);
                g_print("WS askSpecial: ask %s\n", text);
            } else if (!memcmp(type, "new-program", 11)) {
                g_print("WS new-program\n");
            } else if (!memcmp(type, "rdl", 3)) {
                g_print("WS rdl\n");
            } else if (!memcmp(type, "link", 4)) {
                g_print("WS link\n");
            } else if (!memcmp(type, "button", 6)) {
                g_print("WS button\n");
            } else if (!memcmp(type, "video", 5)) {
                g_print("WS video\n");
            } else if (!memcmp(type, "picture", 7)) {
                g_print("WS picture\n");
            } else if (!memcmp(type, "choice", 5)) {
                g_print("WS choice\n");
            } else {
                g_print("WS Unhandled message type: %s\n", type);
            }
        }

        g_object_unref(reader);
        g_object_unref(parser);
    } else {
        g_print("WS Invalid data type: %d\n", type);
    }
}

void genie::wsClient::on_close(SoupWebsocketConnection *conn, gpointer data)
{
    wsClient *obj = reinterpret_cast<wsClient *>(data);
    // soup_websocket_connection_close(conn, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);

    const char *close_data = soup_websocket_connection_get_close_data(conn);

    gushort code = soup_websocket_connection_get_close_code(conn);
    g_print("Genie WebSocket connection closed: %d %s\n", code, close_data);
}

void genie::wsClient::on_connection(SoupSession *session, GAsyncResult *res, gpointer data)
{
    wsClient *obj = reinterpret_cast<wsClient *>(data);

    SoupWebsocketConnection *conn;
    GError *error = NULL;

    conn = soup_session_websocket_connect_finish(session, res, &error);
    if (error) {
        g_print("Error: %s\n", error->message);
        g_error_free(error);
        return;
    }

    soup_websocket_connection_set_max_incoming_payload_size(conn, 512000);
    obj->setConnection(conn);
    obj->acceptStream = false;

    g_signal_connect(conn, "message", G_CALLBACK(genie::wsClient::on_message), data);
    g_signal_connect(conn, "closed",  G_CALLBACK(genie::wsClient::on_close),   data);
}

void genie::wsClient::setConnection(SoupWebsocketConnection *conn)
{
    if (!conn) return;
    wconn = conn;
}

genie::wsClient::wsClient(App *appInstance)
{
    app = appInstance;
    conversationId = NULL;
    accessToken = g_strdup(app->m_config->genieAccessToken);
    url = g_strdup(app->m_config->genieURL);

    tInit = false;
}

genie::wsClient::~wsClient()
{
}

int genie::wsClient::init()
{
    connect();
    return true;
}

void genie::wsClient::connect()
{
    SoupSession *session;
    SoupMessage *msg;

    session = soup_session_new();
    if (!memcmp(url, "wss", 3)) {
        // enable the wss support
        gchar *wss_aliases[] = { (gchar *)"wss", NULL };
        g_object_set(session, SOUP_SESSION_HTTPS_ALIASES, wss_aliases, NULL);
    }

    gchar *convId = (gchar *)"";
    if (app->m_config->conversationId) {
        convId = g_strdup_printf("?id=%s", app->m_config->conversationId);
    }

    gchar *uri = g_strdup_printf("%s%s", url, convId);
    msg = soup_message_new(SOUP_METHOD_GET, uri);
    g_free(uri);

    if (app->m_config->conversationId) {
        g_free(convId);
    }

    if (accessToken) {
        gchar *auth = g_strdup_printf("Bearer %s", accessToken);
        soup_message_headers_append(msg->request_headers, "Authorization", auth);
        g_free(auth);
    }

    soup_session_websocket_connect_async(
            session,
            msg,
            NULL, NULL, NULL,
            (GAsyncReadyCallback)genie::wsClient::on_connection,
            this
    );

    return;
}
