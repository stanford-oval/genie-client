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

#include <string>
#include <unordered_map>
#include <vector>

namespace genie {

/**
 * A tiny, handlebar-like template system.
 */
class TemplateString {
public:
  TemplateString(const char *tmpl);
  static TemplateString from_file(const char *filename);

  std::string
  render(const std::unordered_map<std::string, const char *> &params) const;

private:
  enum class ChunkType { STRING, PARAMETER };
  struct Chunk {
    ChunkType type;
    std::string text;
  };

  std::vector<Chunk> chunks;
};

} // namespace genie
