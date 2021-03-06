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

#include "c-style-callback.hpp"
#include <libsoup/soup.h>

/**
 * Utilities to deal with libsoup and C++ lambdas
 */

namespace genie {
template <typename Callback>
void send_soup_message(SoupSession *session, SoupMessage *msg, Callback cb) {
  auto callback = make_c_async_callback<SoupSession *, SoupMessage *>(
      std::forward<Callback>(cb));
  soup_session_queue_message(session, msg, callback.first, callback.second);
}
} // namespace genie
