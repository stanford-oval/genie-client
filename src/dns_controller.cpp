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

#include "dns_controller.hpp"

#include <cstdio>
#include <memory>
#include <sstream>

genie::DNSController::DNSController(const char *dns_server)
    : dns_server(dns_server) {
  g_debug("Initializing DNS controller...");

  update_dns_config();

  auto_gobject_ptr<GFile> file(g_file_new_for_path("/data/wifi/resolv.conf"),
                               adopt_mode::owned);

  GError *error = nullptr;
  m_file_monitor = auto_gobject_ptr<GFileMonitor>(
      g_file_monitor(file.get(), G_FILE_MONITOR_NONE, nullptr, &error),
      adopt_mode::owned);
  if (error) {
    g_critical("Failed to install file monitor for DNS configuration: %s",
               error->message);
    g_error_free(error);
    return;
  }

  g_signal_connect(m_file_monitor.get(), "changed", G_CALLBACK(on_changed),
                   this);
}

genie::DNSController::~DNSController() {
  if (m_file_monitor)
    g_object_run_dispose(G_OBJECT(m_file_monitor.get()));
}

void genie::DNSController::on_changed(GFileMonitor *monitor, GFile *file,
                                      GFile *other_file,
                                      GFileMonitorEvent event, gpointer data) {
  DNSController *self = static_cast<DNSController *>(data);
  self->update_dns_config();
}

void genie::DNSController::update_dns_config() {
  g_debug("Updating DNS configuration...");

  char *contents;
  size_t size;
  GError *error = nullptr;
  if (!g_file_get_contents("/data/wifi/resolv.conf", &contents, &size,
                           &error)) {
    g_warning("Failed to read new DNS configuration: %s", error->message);
    g_error_free(error);
    return;
  }

  std::unique_ptr<char *, fn_deleter<char *, g_strfreev>> lines(
      g_strsplit(contents, "\n", -1));
  g_free(contents);

  std::ostringstream new_buffer;

  bool any_change = false;
  bool any_nameserver = false;
  for (int i = 0; lines.get()[i]; i++) {
    const char *line = lines.get()[i];
    if (!*line)
      continue;

    if (g_str_has_suffix(line, " 114.114.114.114")) {
      g_debug("Found bad DNS configuration line");
      any_change = true;
      continue;
    }
    if (g_str_has_prefix(line, "nameserver "))
      any_nameserver = true;

    new_buffer << line << '\n';
  }
  if (!any_change)
    return;
  if (!any_nameserver)
    new_buffer << "nameserver " << dns_server << std::endl;

  std::string new_contents(new_buffer.str());
  if (!g_file_set_contents("/data/wifi/resolv.conf", new_contents.c_str(), -1,
                           &error)) {
    g_warning("Failed to write new DNS configuration: %s", error->message);
    g_error_free(error);
    return;
  }
}
