/*
    WmiClient.hpp
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

#ifndef AMITGDEV_WMI_CLIENT_HPP_
#define AMITGDEV_WMI_CLIENT_HPP_

#include <expected>
#include <string>

#include "WmiNamespaceName.hpp"
#include "WmiWrapper.hpp"

namespace amitgdev {

class WmiClient final {
 public:
  // A connection failure is reported here, by the call that caused it, so a
  // WmiClient that exists was connected when it was created. It can still
  // lose the connection later (e.g. the WMI service restarts); that surfaces
  // as an HRESULT from Query(), and the caller recovers by creating a new
  // client.
  [[nodiscard]] static std::expected<WmiClient, Result>
  Create(const WmiNamespaceName& wmi_namespace_name = L"ROOT\\CIMV2");

  // Move is what lets Create() return the object. Copying is deleted because
  // WmiWrapper owns COM pointers, and assignment has no user.
  ~WmiClient() = default;
  WmiClient(const WmiClient&) = delete;
  WmiClient& operator=(const WmiClient&) = delete;
  WmiClient(WmiClient&&) noexcept = default;
  WmiClient& operator=(WmiClient&&) = delete;

  // Failures are returned as an HRESULT instead of an empty result, so "no
  // rows matched" and "the query failed" stay distinct. Only allocation failure
  // is thrown (std::bad_alloc).
  [[nodiscard]] std::expected<Columns, Result> Query(const std::wstring& query,
                                                     const Rows& rows);

 private:
  // Private so a client cannot be built without going through Create(), which
  // is what guarantees it starts connected.
  WmiClient() = default;

  WmiWrapper wmi_wrapper_;
};

}  // namespace amitgdev

#endif  // AMITGDEV_WMI_CLIENT_HPP_
