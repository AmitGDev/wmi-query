/*
    WmiWrapper.hpp
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

#ifndef AMITGDEV_WMI_WRAPPER_HPP_
#define AMITGDEV_WMI_WRAPPER_HPP_

// Based on the 7 steps Microsoft Example: Getting WMI Data from the Local
// Computer Ref.:
// https://docs.microsoft.com/en-us/windows/win32/wmisdk/example--getting-wmi-data-from-the-local-computer

#include <wbemcli.h>
#include <winnt.h>

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "WmiNamespaceName.hpp"

namespace amitgdev {

using Rows = std::vector<std::wstring>;
// The variant leaves room for typed values later; today every property is
// stringified.
using Data = std::pair<std::wstring, std::variant<std::wstring, int>>;
using Column = std::vector<Data>;
using Columns = std::vector<Column>;

class WmiWrapper final {
 public:
  WmiWrapper() = default;

  ~WmiWrapper() { Cleanup(); }

  // The object owns raw COM interface pointers. A shallow copy would release
  // them twice, and a move would need null-state handling nobody needs yet.
  WmiWrapper(const WmiWrapper&) = delete;
  WmiWrapper& operator=(const WmiWrapper&) = delete;
  WmiWrapper(WmiWrapper&&) = delete;
  WmiWrapper& operator=(WmiWrapper&&) = delete;

  // Note: L"ROOT\\CIMV2" constructs a temporary std::wstring => may throw.
  [[nodiscard]] HRESULT Connect() { return Connect(L"ROOT\\CIMV2"); }

  // Replaces any earlier connection. On failure the object stays disconnected,
  // so a half-built connection is never observable.
  [[nodiscard]] HRESULT
  Connect(const WmiNamespaceName& wmi_namespace_name) noexcept;

  // Drops any earlier result set and starts a new query. The HRESULT tells the
  // caller why a query failed, which a bool could not.
  [[nodiscard]] HRESULT ExecQuery(const std::wstring& query) noexcept;

  // Consumes the result set: the enumerator is forward-only, so a second call
  // after a successful read returns nothing. Run ExecQuery again to re-read.
  [[nodiscard]] Columns GetColumns(const Rows& rows) const;

  // Initializes COM (multithreaded) and the process-wide COM security. Every
  // successful call must be balanced by one Uninitialize() on the same thread.
  // A failure leaves nothing initialized, so there is nothing to unwind.
  [[nodiscard]] static HRESULT Initialize() noexcept;

  // Balances one successful Initialize() on the calling thread. Without a
  // matching Initialize() it does nothing.
  static void Uninitialize() noexcept;

 private:
  void Cleanup() noexcept;

  IWbemServices* svc_{nullptr};
  IWbemLocator* loc_{nullptr};

  // COM initialization is per thread, so the count of unbalanced Initialize()
  // calls is too.
  inline static thread_local int init_depth_{0};
  IEnumWbemClassObject* enumerator_{nullptr};
};

}  // namespace amitgdev

#endif  // AMITGDEV_WMI_WRAPPER_HPP_
