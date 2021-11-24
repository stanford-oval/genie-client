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

#include "webserver.hpp"
#include "app.hpp"
#include "string.h"

#include "config.h"
#define GETTEXT_PACKAGE "genie-client"
#include <glib/gi18n-lib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::WebServer"

static const char *html_template_1 = "<!DOCTYPE html>"
                                     "<html>"
                                     "<head>"
                                     "<title>";
static const char *html_template_2 =
    "</title>"
    "<meta charset=\"utf-8\" />"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />"
    "<link "
    "href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/"
    "bootstrap.min.css\" rel=\"stylesheet\" "
    "integrity=\"sha384-"
    "1BmE4kWBq78iYhFldvKuhfTAU6auU8tT94WrHftjDbrCEXSU1oBoqyl2QvZ6jIW3\" "
    "crossorigin=\"anonymous\" />"
    "<link href=\"/css/style.css\" rel=\"stylesheet\" />"
    "</head>"
    "<body>"
    "<div class=\"container\">";
static const char *html_template_3 =
    "</div>"
    "<script "
    "src=\"https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/js/"
    "bootstrap.bundle.min.js\" "
    "integrity=\"sha384-ka7Sk0Gln4gmtz2MlQnikT1wXgYsOg+OMhuP+IlRH9sENBO0LRn5q+"
    "8nbTov4+1p\" crossorigin=\"anonymous\"></script>"
    "<script src=\"/js/shared.js\"></script>"
    "</body>"
    "</html>";

static const char *title_error = N_("Genie - Error");
static const char *title_normal = N_("Genie Configuration");

static const char *reply_403 =
    N_("<h1>Forbidden</h1><p>The requested page is not accessible.</p>");
static const char *reply_404 =
    N_("<h1>Not Found</h1><p>The requested page does not exist.</p>");

genie::WebServer::WebServer(App *app)
    : app(app),
      server(soup_server_new("server-header",
                             PACKAGE_NAME "/" PACKAGE_VERSION " ", nullptr),
             adopt_mode::owned) {
  GError *error = nullptr;
  soup_server_listen_all(server.get(), app->config->webui_port,
                         (SoupServerListenOptions)0, &error);
  if (error) {
    g_critical("Failed to initialize Web UI HTTP server: %s", error->message);
    g_error_free(error);
    return;
  }

  soup_server_add_handler(
      server.get(), "/favicon.ico",
      [](SoupServer *server, SoupMessage *msg, const char *path,
         GHashTable *query, SoupClientContext *context, gpointer data) {
        WebServer *self = static_cast<WebServer *>(data);
        self->handle_asset(msg, path);
      },
      this, nullptr);
  soup_server_add_handler(
      server.get(), "/css",
      [](SoupServer *server, SoupMessage *msg, const char *path,
         GHashTable *query, SoupClientContext *context, gpointer data) {
        WebServer *self = static_cast<WebServer *>(data);
        self->handle_asset(msg, path);
      },
      this, nullptr);
  soup_server_add_handler(
      server.get(), "/js",
      [](SoupServer *server, SoupMessage *msg, const char *path,
         GHashTable *query, SoupClientContext *context, gpointer data) {
        WebServer *self = static_cast<WebServer *>(data);
        self->handle_asset(msg, path);
      },
      this, nullptr);
  soup_server_add_handler(
      server.get(), "/",
      [](SoupServer *server, SoupMessage *msg, const char *path,
         GHashTable *query, SoupClientContext *context, gpointer data) {
        WebServer *self = static_cast<WebServer *>(data);
        if (strcmp(path, "/") == 0)
          self->handle_index(msg);
        else
          self->handle_404(msg, path);
      },
      this, nullptr);

  g_message("Web UI listening on port %d", app->config->webui_port);
}

void genie::WebServer::handle_asset(SoupMessage *msg, const char *path) {
  // security check against path traversal attacks
  if (g_str_has_prefix(path, ".") || g_str_has_prefix(path, "/.") ||
      strstr(path, "..")) {
    handle_404(msg, path);
    return;
  }

  char *filename =
      g_build_filename(app->config->asset_dir, "webui", path, nullptr);
  g_debug("Serving asset from %s", filename);
  GError *error = nullptr;
  gchar *contents;
  gsize length;
  if (!g_file_get_contents(filename, &contents, &length, &error)) {
    g_free(filename);

    if (error->code == G_FILE_ERROR_ACCES || error->code == G_FILE_ERROR_PERM) {
      log_request(path, 403);
      send_html(msg, 403, title_error, reply_403);
    } else {
      handle_404(msg, path);
    }
    return;
  }
  g_free(filename);

  const char *content_type = "application/octet-stream";
  if (g_str_has_prefix(path, "/css"))
    content_type = "text/css";
  else if (g_str_has_prefix(path, "/js"))
    content_type = "application/javascript";
  else if (strcmp(path, "/favicon.ico") == 0)
    content_type = "image/png";

  log_request(path, 200);
  soup_message_set_status(msg, 200);
  soup_message_set_response(msg, content_type, SOUP_MEMORY_TAKE, contents,
                            length);
}

void genie::WebServer::handle_404(SoupMessage *msg, const char *path) {
  log_request(path, 404);
  send_html(msg, 404, title_error, reply_404);
}

void genie::WebServer::handle_index(SoupMessage *msg) {
  SoupURI *genie_url = soup_uri_new(app->config->genie_url);

  SoupURI *toplevel_url = soup_uri_new_with_base(genie_url, "/");
  if (strcmp(soup_uri_get_scheme(genie_url), "wss") == 0)
    soup_uri_set_scheme(toplevel_url, "https");
  else
    soup_uri_set_scheme(toplevel_url, "http");
  soup_uri_free(genie_url);

  gchar *toplevel_urlstr = soup_uri_to_string(toplevel_url, false);
  soup_uri_free(toplevel_url);
  gchar *body = g_strdup_printf(
      _("<h1>Genie Configuration</h1><p>Your Genie speaker is "
        "connected successfully to <a href=\"%1$s\">%1$s</a>.</p>"),
      toplevel_urlstr);
  g_free(toplevel_urlstr);

  log_request("/", 200);
  send_html(msg, 200, title_normal, body);
  g_free(body);
}

void genie::WebServer::send_html(SoupMessage *msg, int status,
                                 const char *page_title,
                                 const char *page_body) {
  gchar *buffer = (gchar *)g_malloc(
      strlen(html_template_1) + strlen(page_title) + strlen(html_template_2) +
      strlen(page_body) + strlen(html_template_3));

  gchar *write_at = buffer;
  strcpy(write_at, html_template_1);
  write_at += strlen(html_template_1);
  strcpy(write_at, page_title);
  write_at += strlen(page_title);
  strcpy(write_at, html_template_2);
  write_at += strlen(html_template_2);
  strcpy(write_at, page_body);
  write_at += strlen(page_body);
  strcpy(write_at, html_template_3);
  write_at += strlen(html_template_3);

  soup_message_set_status(msg, status);
  soup_message_set_response(msg, "text/html", SOUP_MEMORY_TAKE, buffer,
                            write_at - buffer);
}

void genie::WebServer::log_request(const char *path, int status) {
  g_message("GET %s - %d", path, status);
}
