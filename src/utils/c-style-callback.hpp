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

#include <functional>
#include <glib.h>

/**
 * Wrap a C++ lambda into a C-style callback whose last argument is a void*
 */
namespace genie {

template <typename... Args, typename Lambda>
auto make_c_async_callback(Lambda lambda) {
  // move into a dynamically allocated std::function
  auto fn = new std::function<void(Args...)>(std::move(lambda));
  // the trick we're playing here is that a C++ lambda with no captures
  // is equivalent to a plain C function, and we can pass it by function
  // pointer
  return std::make_pair(
      [](Args... args, void *user_data) {
        auto fn = static_cast<std::function<void(Args...)> *>(user_data);
        (*fn)(args...);
        delete fn;
      },
      fn);
}

} // namespace genie
