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

#include <gst/gst.h>

namespace genie {

enum class adopt_mode {
    owned,
    ref,
    ref_sink,
};

template<typename T>
class auto_gst_ptr {
private:
    T* m_instance;

public:
    auto_gst_ptr() : m_instance(nullptr) {}

    auto_gst_ptr(T* instance, adopt_mode mode) : m_instance(instance) {
        if (m_instance) {
            if (mode == adopt_mode::ref)
                gst_object_ref(m_instance);
            else if (mode == adopt_mode::ref_sink)
                gst_object_ref_sink(m_instance);
        }
    }

    auto_gst_ptr(const auto_gst_ptr& other) : m_instance(other.m_instance) {
        if (m_instance)
            gst_object_ref(m_instance);
    }

    ~auto_gst_ptr() {
        if (m_instance)
            gst_object_unref(m_instance);
    }

    auto_gst_ptr<T>& operator=(const auto_gst_ptr<T>& other) {
        if (other.m_instance)
            gst_object_ref(other.m_instance);
        this->~auto_gst_ptr();
        m_instance = other.m_instance;
        return *this;
    };

    T* get() const {
        return m_instance;
    };

    T& operator->() {
        return *m_instance;
    }
    const T& operator->() const {
        return *m_instance;
    }

    explicit operator bool() {
        return !!m_instance;
    }
};

}
