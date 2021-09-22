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

#include <glib-object.h>
#include <gst/gst.h>

namespace genie {

enum class adopt_mode {
  owned,
  ref,
  ref_sink,
};

template <typename T> class auto_gobject_ptr {
private:
  T *m_instance;

public:
  auto_gobject_ptr() : m_instance(nullptr) {}

  auto_gobject_ptr(T *instance, adopt_mode mode) : m_instance(instance) {
    if (m_instance) {
      if (mode == adopt_mode::ref)
        g_object_ref(m_instance);
      else if (mode == adopt_mode::ref_sink)
        g_object_ref_sink(m_instance);
    }
  }

  auto_gobject_ptr(const auto_gobject_ptr &other)
      : m_instance(other.m_instance) {
    if (m_instance)
      g_object_ref(m_instance);
  }
  auto_gobject_ptr(auto_gobject_ptr &&other) {
    m_instance = other.m_instance;
    other.m_instance = nullptr;
  }

  ~auto_gobject_ptr() {
    if (m_instance)
      g_object_unref(m_instance);
  }

  auto_gobject_ptr<T> &operator=(const auto_gobject_ptr<T> &other) {
    if (other.m_instance)
      g_object_ref(other.m_instance);
    if (m_instance)
      g_object_unref(m_instance);
    m_instance = other.m_instance;
    return *this;
  };
  auto_gobject_ptr<T> &operator=(auto_gobject_ptr<T> &&other) {
    if (m_instance == other.m_instance) {
      return *this;
    }
    if (m_instance)
      g_object_unref(m_instance);
    m_instance = other.m_instance;
    other.m_instance = nullptr;
    return *this;
  };
  auto_gobject_ptr<T> &operator=(nullptr_t ptr) {
    if (m_instance)
      g_object_unref(m_instance);
    m_instance = nullptr;
    return *this;
  }

  T *get() const { return m_instance; };

  T &operator->() { return *m_instance; }
  const T &operator->() const { return *m_instance; }

  explicit operator bool() { return !!m_instance; }
};

template <typename T, void (*deleter)(T *)> struct fn_deleter {
  void operator()(T *obj) { deleter(obj); }
};

} // namespace genie
