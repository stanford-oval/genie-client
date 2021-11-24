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

#include "template-string.hpp"
#include <glib.h>
#include <sstream>
#include <stdarg.h>

genie::TemplateString::TemplateString(const char *tmpl) {
  GError *error = nullptr;
  GRegex *regex = g_regex_new("\\{\\{\\s*([a-zA-Z0-9_]+)\\s*\\}\\}",
                              G_REGEX_OPTIMIZE, (GRegexMatchFlags)0, &error);
  if (error) {
    g_error("Failed to initialize template string parser: %s", error->message);
    return;
  }

  GMatchInfo *match_info;
  g_regex_match(regex, tmpl, (GRegexMatchFlags)0, &match_info);

  int previous_chunk_end = 0;
  while (g_match_info_matches(match_info)) {
    int start_pos, end_pos;
    g_match_info_fetch_pos(match_info, 0, &start_pos, &end_pos);
    if (start_pos > previous_chunk_end) {
      chunks.emplace_back(
          Chunk{ChunkType::STRING,
                std::string(tmpl + previous_chunk_end, tmpl + start_pos)});
    }

    int name_start_pos, name_end_pos;
    g_match_info_fetch_pos(match_info, 1, &name_start_pos, &name_end_pos);

    chunks.emplace_back(
        Chunk{ChunkType::PARAMETER,
              std::string(tmpl + name_start_pos, tmpl + name_end_pos)});

    previous_chunk_end = end_pos;
    g_match_info_next(match_info, nullptr);
  }
  if (previous_chunk_end < (int)strlen(tmpl)) {
    chunks.emplace_back(
        Chunk{ChunkType::STRING,
              std::string(tmpl + previous_chunk_end, tmpl + strlen(tmpl))});
  }
}

std::string genie::TemplateString::render(
    const std::unordered_map<std::string, const char *> &params) const {
  std::ostringstream str;

  for (const auto &chunk : chunks) {
    if (chunk.type == ChunkType::PARAMETER) {
      auto it = params.find(chunk.text);
      if (it == params.end())
        g_warning("Missing template parameter %s", chunk.text.c_str());
      else
        str << it->second;
    } else {
      str << chunk.text;
    }
  }

  return std::move(str.str());
}

genie::TemplateString genie::TemplateString::from_file(const char *filename) {
  GError *error = nullptr;
  gchar *contents;
  gsize length;
  if (!g_file_get_contents(filename, &contents, &length, &error)) {
    g_critical("Failed to load template file from %s: %s", filename,
               error->message);
    g_error_free(error);
    return genie::TemplateString("");
  }

  TemplateString tmpl(contents);
  g_free(contents);
  return tmpl;
}
