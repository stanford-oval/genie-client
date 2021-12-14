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
#include "utils/c-style-callback.hpp"
#include "utils/soup-utils.hpp"
#include <fcntl.h>
#include <functional>
#include <json-glib/json-glib.h>

#include "config.h"
#define GETTEXT_PACKAGE "genie-client"
#include <glib/gi18n-lib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "genie::WebServer"

using namespace kainjow;

static const char *title_error = N_("Genie - Error");
static const char *title_normal = N_("Genie Configuration");

static const char *reply_403 =
    N_("<h1>Forbidden</h1><p>The requested page is not accessible.</p>");
static const char *reply_404 =
    N_("<h1>Not Found</h1><p>The requested page does not exist.</p><p><a "
       "href=\"/\">Home page</a></p>");
static const char *reply_405 = N_("<h1>Method Not Allowed</h1>");

static gchar *gen_random(size_t size) {
  guchar *buffer = (guchar *)g_malloc(size);
  int fd = open("/dev/urandom", O_RDONLY);
  read(fd, buffer, size);
  close(fd);
  char *base64 = g_base64_encode(buffer, size);
  g_free(buffer);
  return base64;
}

static mustache::mustache load_html_template(genie::App *app,
                                             const char *filename) {
  gchar *pathname =
      g_build_filename(app->config->asset_dir, "webui", filename, nullptr);
  GError *error = nullptr;
  gchar *contents;
  gsize length;
  if (!g_file_get_contents(pathname, &contents, &length, &error)) {
    g_critical("Failed to load template file from %s: %s", filename,
               error->message);
    g_free(pathname);
    g_error_free(error);
    return mustache::mustache("");
  }

  mustache::mustache tmpl{contents};
  g_free(contents);
  g_free(pathname);
  return tmpl;
}

genie::WebServer::WebServer(App *app)
    : app(app),
      server(soup_server_new("server-header",
                             PACKAGE_NAME "/" PACKAGE_VERSION " ", nullptr),
             adopt_mode::owned),
      page_layout(load_html_template(app, "layout.html")) {

  gchar *tmp = gen_random(32);
  csrf_token = tmp;
  g_free(tmp);

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
      server.get(), "/oauth-redirect",
      [](SoupServer *server, SoupMessage *msg, const char *path,
         GHashTable *query, SoupClientContext *context, gpointer data) {
        WebServer *self = static_cast<WebServer *>(data);
        self->handle_oauth_redirect(msg, query);
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
  if (check_method(msg, path, (int)AllowedMethod::GET) != AllowedMethod::GET)
    return;

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
    g_error_free(error);

    if (error->code == G_FILE_ERROR_ACCES || error->code == G_FILE_ERROR_PERM) {
      log_request(msg, path, 403);
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

  log_request(msg, path, 200);
  soup_message_set_status(msg, 200);
  soup_message_set_response(msg, content_type, SOUP_MEMORY_TAKE, contents,
                            length);
}

void genie::WebServer::handle_404(SoupMessage *msg, const char *path) {
  log_request(msg, path, 404);
  send_html(msg, 404, title_error, reply_404);
}

genie::WebServer::AllowedMethod
genie::WebServer::check_method(SoupMessage *msg, const char *path,
                               int allowed_methods) {
  char *method;
  g_object_get(msg, "method", &method, nullptr);
  if (strcmp(method, "GET") == 0) {
    g_free(method);
    if (allowed_methods & (int)AllowedMethod::GET)
      return AllowedMethod::GET;
    handle_405(msg, path);
    return AllowedMethod::NONE;
  } else if (strcmp(method, "POST") == 0) {
    g_free(method);
    if (allowed_methods & (int)AllowedMethod::POST)
      return AllowedMethod::POST;
    handle_405(msg, path);
    return AllowedMethod::NONE;
  } else {
    g_free(method);
    handle_405(msg, path);
    return AllowedMethod::NONE;
  }
}

void genie::WebServer::handle_405(SoupMessage *msg, const char *path) {
  log_request(msg, path, 405);
  send_html(msg, 405, title_error, reply_405);
}

void genie::WebServer::handle_oauth_redirect(SoupMessage *msg,
                                             GHashTable *query) {
  if (check_method(msg, "/oauth-redirect", (int)AllowedMethod::GET) ==
      AllowedMethod::NONE)
    return;

  const char *code = (const char *)g_hash_table_lookup(query, "code");
  if (code == nullptr) {
    // user cancelled authentication
    log_request(msg, "/oauth-redirect", 303);
    soup_message_set_redirect(msg, 303, "/");
    return;
  }

  soup_server_pause_message(server.get(), msg);

  SoupURI *token_uri = soup_uri_new(app->config->genie_url);
  if (strcmp(soup_uri_get_scheme(token_uri), "wss") == 0)
    soup_uri_set_scheme(token_uri, "https");
  else
    soup_uri_set_scheme(token_uri, "http");
  soup_uri_set_path(token_uri, "/me/api/oauth2/token");

  SoupMessage *token_request =
      soup_message_new_from_uri(SOUP_METHOD_POST, token_uri);
  soup_uri_free(token_uri);

  SoupURI *redirect_uri = soup_uri_copy(soup_message_get_uri(msg));
  soup_uri_set_path(redirect_uri, "/oauth-redirect");
  soup_uri_set_query(redirect_uri, nullptr);
  char *redirect_uri_str = soup_uri_to_string(redirect_uri, false);
  soup_uri_free(redirect_uri);

  char *body =
      soup_form_encode("client_id", OAUTH_CLIENT_ID, "client_secret",
                       OAUTH_CLIENT_SECRET, "grant_type", "authorization_code",
                       "code", code, "redirect_uri", redirect_uri_str, nullptr);
  g_free(redirect_uri_str);

  soup_message_set_request(token_request, "application/x-www-form-urlencoded",
                           SOUP_MEMORY_TAKE, body, strlen(body));

  send_soup_message(
      app->get_soup_session(), token_request,
      [msg, this](SoupSession *session, SoupMessage *response) {
        guint status_code;
        GBytes *body;
        gsize size;

        g_object_get(response, "status-code", &status_code,
                     "response-body-data", &body, nullptr);

        g_message("Sent access token request to Genie Cloud, got HTTP %u",
                  status_code);

        if (status_code >= 400) {
          g_warning("Failed to get access token from Genie Cloud: %s",
                    (const char *)g_bytes_get_data(body, &size));
          send_error(msg, 500, "Failed to obtain access token");
          soup_server_unpause_message(server.get(), msg);
          return;
        }

        JsonParser *parser = json_parser_new();
        GError *error = nullptr;
        const char *data = (const char *)g_bytes_get_data(body, &size);
        json_parser_load_from_data(parser, data, size, &error);
        if (error) {
          g_warning("Failed to parse access token reply from Genie: %s",
                    error->message);
          g_warning("Response data: %s", data);
          send_error(msg, 500, error->message);
          soup_server_unpause_message(server.get(), msg);
          return;
        }

        app->config->set_auth_mode(AuthMode::OAUTH2);

        JsonReader *reader = json_reader_new(json_parser_get_root(parser));

        json_reader_read_member(reader, "access_token");
        const char *access_token = json_reader_get_string_value(reader);
        app->set_temporary_access_token(access_token);
        json_reader_end_member(reader); // access_token

        json_reader_read_member(reader, "refresh_token");
        const char *refresh_token = json_reader_get_string_value(reader);
        json_reader_end_member(reader); // refresh_token
        app->config->set_genie_access_token(refresh_token);

        app->config->save();
        app->force_reconnect();

        g_object_unref(parser);
        g_object_unref(reader);
        g_bytes_unref(body);

        soup_message_set_redirect(msg, 303, "/");
        soup_server_unpause_message(server.get(), msg);
      });
}

void genie::WebServer::handle_index(SoupMessage *msg) {
  auto method = check_method(
      msg, "/", (int)AllowedMethod::GET | (int)AllowedMethod::POST);
  if (method == AllowedMethod::GET)
    handle_index_get(msg);
  else if (method == AllowedMethod::POST)
    handle_index_post(msg);
}

void genie::WebServer::handle_index_post(SoupMessage *msg) {
  GHashTable *fields = nullptr;
  bool any_change = false;
  bool needs_reconnect = false;
  bool needs_oauth_redirect = false;

  if (g_strcmp0(
          soup_message_headers_get_content_type(msg->request_headers, nullptr),
          "application/x-www-form-urlencoded") != 0) {
    log_request(msg, "/", 406);
    send_html(msg, 406, title_error, "<h1>Not Acceptable</h1>");
    goto out;
  }

  fields = soup_form_decode(msg->request_body->data);

  if (g_strcmp0((const char *)g_hash_table_lookup(fields, "_csrf"),
                csrf_token.c_str()) != 0) {
    log_request(msg, "/", 403);
    send_html(msg, 403, title_error, "<h1>Invalid CSRF token</h1>");
    goto out;
  }

  {
    const char *url = (const char *)g_hash_table_lookup(fields, "url");
    if (url && *url && g_strcmp0(url, app->config->genie_url) != 0) {
      app->config->set_genie_url(url);
      any_change = true;
      needs_reconnect = true;
    }
  }
  {
    const char *auth_mode =
        (const char *)g_hash_table_lookup(fields, "auth_mode");
    if (auth_mode && *auth_mode) {
      AuthMode parsed = Config::parse_auth_mode(auth_mode);
      if (parsed != app->config->auth_mode) {
        app->config->set_auth_mode(parsed);
        any_change = true;
        needs_reconnect = true;

        if (parsed == AuthMode::OAUTH2)
          needs_oauth_redirect = true;
      }
    }
  }
  {
    const char *access_token =
        (const char *)g_hash_table_lookup(fields, "access_token");
    if (access_token && *access_token &&
        g_strcmp0(access_token, app->config->genie_access_token) != 0) {
      app->config->set_genie_access_token(access_token);
      any_change = true;
      needs_reconnect = true;
    }
  }
  {
    const char *conversation_id =
        (const char *)g_hash_table_lookup(fields, "conversation_id");
    if (conversation_id && *conversation_id &&
        g_strcmp0(conversation_id, app->config->conversation_id) != 0) {
      app->config->set_conversation_id(conversation_id);
      any_change = true;
      needs_reconnect = true;
    }
  }

  if (any_change)
    app->config->save();
  if (app->config->auth_mode == AuthMode::OAUTH2 &&
      (!app->config->genie_access_token || !*app->config->genie_access_token))
    needs_oauth_redirect = true;

  if (needs_oauth_redirect) {
    SoupURI *redirect_uri = soup_uri_copy(soup_message_get_uri(msg));
    soup_uri_set_path(redirect_uri, "/oauth-redirect");
    char *redirect_uri_str = soup_uri_to_string(redirect_uri, false);
    soup_uri_free(redirect_uri);

    SoupURI *oauth_login_url = soup_uri_new(app->config->genie_url);
    soup_uri_set_scheme(oauth_login_url, "http");
    soup_uri_set_path(oauth_login_url, "/me/api/oauth2/authorize");
    soup_uri_set_query_from_fields(oauth_login_url, "response_type", "code",
                                   "client_id", OAUTH_CLIENT_ID, "redirect_uri",
                                   redirect_uri_str, "scope",
                                   "profile user-exec-command", nullptr);
    char *oauth_login_url_str = soup_uri_to_string(oauth_login_url, false);
    soup_uri_free(oauth_login_url);
    g_free(redirect_uri_str);

    soup_message_set_redirect(msg, 303, oauth_login_url_str);
    g_free(oauth_login_url_str);
    goto out;
  }

  if (needs_reconnect)
    app->force_reconnect();

  handle_index_get(msg);

out:
  if (fields)
    g_hash_table_unref(fields);
}

void genie::WebServer::handle_index_get(SoupMessage *msg) {
  const char *auth_mode = Config::auth_mode_to_string(app->config->auth_mode);

  mustache::object data{
      {"csrf_token", csrf_token.c_str()},
      {"url", app->config->genie_url},
      {"auth_mode_checked", mustache::lambda([auth_mode](const std::string &s) {
         if (s == auth_mode)
           return "selected";
         else
           return "";
       })},
      {"access_token",
       app->config->genie_access_token ? app->config->genie_access_token : ""},
      {"conversation_id", app->config->conversation_id}};
  auto body = load_html_template(app, "config.html").render(data);

  log_request(msg, "/", 200);
  send_html(msg, 200, title_normal, body.c_str());
}

void genie::WebServer::send_html(SoupMessage *msg, int status,
                                 const char *page_title,
                                 const char *page_body) {
  mustache::object data{{"page_title", page_title}, {"page_body", page_body}};
  auto rendered = page_layout.render(data);

  soup_message_set_status(msg, status);
  soup_message_set_response(msg, "text/html", SOUP_MEMORY_COPY, rendered.data(),
                            rendered.size());
}

void genie::WebServer::send_error(SoupMessage *msg, int status,
                                  const char *error) {
  auto body = g_strdup_printf(
      _("<h1>Error</h1><p>Sorry, that did not work: %s</p>"), error);
  send_html(msg, status, title_error, body);
  g_free(body);
}

void genie::WebServer::log_request(SoupMessage *msg, const char *path,
                                   int status) {
  char *method;
  g_object_get(msg, "method", &method, nullptr);
  g_message("%s %s - %d", method, path, status);
  g_free(method);
}
