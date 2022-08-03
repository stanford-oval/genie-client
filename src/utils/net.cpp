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

#include "net.hpp"

#include <stdio.h>
#include <stdlib.h>

#include <net/if.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

genie::NetController::NetController(App *app) : app(app) {}

genie::NetController::~NetController() {}

genie::WifiAuthMode genie::NetController::parse_auth_mode(const char *auth_mode) {
  if (strcmp(auth_mode, "open") == 0) {
    return genie::WifiAuthMode::OPEN;
  } else if (strcmp(auth_mode, "wep") == 0) {
    return genie::WifiAuthMode::WEP;
  } else if (strcmp(auth_mode, "wpa") == 0) {
    return genie::WifiAuthMode::WPA;
  } else {
    g_warning("Invalid authentication mode %s, using default 'none'",
              auth_mode);
    return genie::WifiAuthMode::OPEN;
  }
}

bool genie::NetController::set_wifi_config(WifiAuthMode mode, const char *ssid,
                                           const char *secret) {
  FILE *fp;

  fp = fopen("/tmp/wpa_supplicant.conf", "w");
  if (!fp) {
    return false;
  }

  fprintf(fp,
          "ctrl_interface=/var/run/wpa_supplicant\n"
          "update_config=1\ncountry=%s\nnetwork={\n", "US");
  fprintf(fp, "\tssid=\"%s\"\n", ssid);
  if (mode == WifiAuthMode::OPEN || mode == WifiAuthMode::WEP) {
    fprintf(fp, "\tkey_mgmt=NONE\n");
    if (mode == WifiAuthMode::WEP) {
      fprintf(fp, "\twep_key0=\"%s\"\nwep_tx_keyidx=0\n", secret);
    }
  } else if (mode == WifiAuthMode::WPA) {
    fprintf(fp, "\tpsk=\"%s\"\n", secret);
  }
  fprintf(fp, "}\n");

  fclose(fp);
  return true;
}

bool genie::NetController::get_mac_address(char *ifname, char **outbuf) {
  struct ifreq ifr;
  char buf[1024];
  int success = 0;

  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock == -1) {
    return false;
  }

  strncpy(ifr.ifr_name, ifname, sizeof(buf) - 1);
  if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
      memcpy(outbuf, ifr.ifr_hwaddr.sa_data, 6);
      close(sock);
      return true;
    }
  } else {
    /* handle error */
  }

  close(sock);
  return false;
}

bool genie::NetController::start_ap() {
  if (!app->config->net_wlan_ap_ctrl || !strlen(app->config->net_wlan_ap_ctrl)) {
    return false;
  }
  char *helper_path =
      g_build_filename(app->config->asset_dir, app->config->net_wlan_ap_ctrl, nullptr);

  if (!app->config->net_wlan_if) {
    return false;
  }
  char *buf = (char *)malloc(16);
  if (!get_mac_address(app->config->net_wlan_if, &buf)) {
    g_free(buf);
    return false;
  }

  gchar *cmd =
      g_strdup_printf("%s start %s \"Genie Speaker %02x%02x%02x\"", helper_path,
                      app->config->net_wlan_if, buf[3], buf[4], buf[5]);
  g_free(buf);
  g_free(helper_path);

  int rc = system(cmd);
  g_free(cmd);
  return rc == 0 ? true : false;
}

bool genie::NetController::stop_ap() {
  if (!app->config->net_wlan_ap_ctrl || !strlen(app->config->net_wlan_ap_ctrl)) {
    return false;
  }
  char *helper_path =
      g_build_filename(app->config->net_wlan_ap_ctrl, nullptr);
  gchar *cmd = g_strdup_printf("%s stop %s", helper_path, app->config->net_wlan_if);
  g_free(helper_path);

  int rc = system(cmd);
  g_free(cmd);
  return rc == 0 ? true : false;
}

bool genie::NetController::start_sta() {
  if (!app->config->net_wlan_sta_ctrl || !strlen(app->config->net_wlan_sta_ctrl)) {
    return false;
  }
  char *helper_path =
      g_build_filename(app->config->net_wlan_sta_ctrl, nullptr);
  gchar *cmd = g_strdup_printf("%s start %s", helper_path, app->config->net_wlan_if);
  g_free(helper_path);

  int rc = system(cmd);
  g_free(cmd);
  return rc == 0 ? true : false;
}

bool genie::NetController::stop_sta() {
  if (!app->config->net_wlan_sta_ctrl || !strlen(app->config->net_wlan_sta_ctrl)) {
    return false;
  }
  char *helper_path =
      g_build_filename(app->config->net_wlan_sta_ctrl, nullptr);
  gchar *cmd = g_strdup_printf("%s stop %s", helper_path, app->config->net_wlan_if);
  g_free(helper_path);

  int rc = system(cmd);
  g_free(cmd);
  return rc == 0 ? true : false;
}
