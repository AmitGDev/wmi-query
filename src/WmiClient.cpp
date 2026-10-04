/*
    WmiClient.cpp
    Copyright (c) 2024-2026, Amit Gefen

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include "WmiClient.hpp"

#include <winerror.h>

#include <string>

#include "WmiNamespaceName.hpp"
#include "WmiWrapper.hpp"

namespace amitgdev {

WmiClient::WmiClient() : WmiClient(L"ROOT\\CIMV2") {}

// A failed connection is not fatal here: connected_ stays false and Query()
// degrades to an empty result, so callers need no exception handling.
WmiClient::WmiClient(const WmiNamespaceName& wmi_namespace_name) {
  if (SUCCEEDED(wmi_wrapper_.Connect(wmi_namespace_name))) {
    connected_ = true;
  }
}

Columns WmiClient::Query(const std::wstring& query, const Rows& rows) {
  // Short-circuiting keeps ExecQuery from touching a null service when the
  // connection never succeeded.
  if (connected_ && SUCCEEDED(wmi_wrapper_.ExecQuery(query))) {
    return wmi_wrapper_.GetColumns(rows);
  }

  return {};
}

}  // namespace amitgdev