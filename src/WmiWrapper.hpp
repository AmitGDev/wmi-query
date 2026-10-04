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

#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "WmiNamespaceName.hpp"

// Forward declarations keep the COM headers out of every includer.
struct IWbemServices;
struct IWbemLocator;
struct IEnumWbemClassObject;

namespace amitgdev {

using Result = int32_t;
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

  // The object owns raw COM interface pointers, so a copy would release them
  // twice. Move construction exists so WmiClient::Create() can return an
  // already connected object; the source is left disconnected, so only one
  // owner releases the pointers. Move assignment has no user yet.
  WmiWrapper(const WmiWrapper&) = delete;
  WmiWrapper& operator=(const WmiWrapper&) = delete;

  WmiWrapper(WmiWrapper&& other) noexcept
      : svc_(std::exchange(other.svc_, nullptr)),
        loc_(std::exchange(other.loc_, nullptr)),
        enumerator_(std::exchange(other.enumerator_, nullptr)) {}

  WmiWrapper& operator=(WmiWrapper&&) = delete;

  // Note: L"ROOT\\CIMV2" constructs a temporary std::wstring => may throw.
  [[nodiscard]] Result Connect() { return Connect(L"ROOT\\CIMV2"); }

  // Replaces any earlier connection. On failure the object stays disconnected,
  // so a half-built connection is never observable.
  [[nodiscard]] Result
  Connect(const WmiNamespaceName& wmi_namespace_name) noexcept;

  // Drops any earlier result set and starts a new query. The HRESULT tells the
  // caller why a query failed, which a bool could not.
  [[nodiscard]] Result ExecQuery(const std::wstring& query) noexcept;

  // Consumes the result set: the enumerator is forward-only, so a second call
  // after a successful read returns nothing. Run ExecQuery again to re-read.
  // ExecQuery returns immediately, so query errors (bad class, bad WQL, access
  // denied) surface here. On failure no partial data is returned, so a
  // truncated result set can never be mistaken for a complete one.
  // Errors are returned, never thrown; the one exception is allocation failure,
  // which propagates as std::bad_alloc.
  [[nodiscard]] std::expected<Columns, Result>
  GetColumns(const Rows& rows) const;

  // Initializes COM (multithreaded) and the process-wide COM security. Every
  // successful call must be balanced by one Uninitialize() on the same thread.
  // A failure leaves nothing initialized, so there is nothing to unwind.
  [[nodiscard]] static Result Initialize() noexcept;

  // Balances one successful Initialize() on the calling thread. Without a
  // matching Initialize() it does nothing. Every WmiWrapper (and so every
  // WmiClient) on this thread must be destroyed first: releasing a COM
  // interface after COM is uninitialized is undefined behavior.
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
