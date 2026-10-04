/*
    WmiNamespace.hpp
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

#ifndef AMITGDEV_WMI_NAMESPACE_HPP_
#define AMITGDEV_WMI_NAMESPACE_HPP_

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "WmiClassName.hpp"
#include "WmiNamespaceName.hpp"
#include "WmiWrapper.hpp"

namespace amitgdev {

// Turns the text of one property into display text. It knows nothing about
// WMI, so it can be a free function, or a lambda.
using Formatter = std::function<std::wstring(const std::wstring&)>;

// Names the property a formatter applies to.
struct RowFormatter {
  std::wstring row_title;
  Formatter format;
};

using RowFormatters = std::vector<RowFormatter>;

class WmiNamespace final {
 public:
  // The formatters run after every Query(), in list order.
  // A formatter for a property that is not in rows has no effect.

  WmiNamespace(WmiNamespaceName wmi_namespace_name, WmiClassName wmi_class_name,
               Rows rows, RowFormatters formatters = {});

  WmiNamespace(WmiClassName wmi_class_name, Rows rows,
               RowFormatters formatters = {})
      : WmiNamespace(L"ROOT\\CIMV2", std::move(wmi_class_name), std::move(rows),
                     std::move(formatters)) {}

  // Runs the query and returns the number of objects found, or the HRESULT of
  // the failure (connection, WQL or provider error). "Found nothing" (0) and
  // "the query failed" stay distinct. Data() is cleared first, so on failure it
  // is empty and never holds the previous result. Only allocation failure is
  // thrown (std::bad_alloc).
  [[nodiscard]] std::expected<size_t, Result> Query();

  // The reference is invalidated by the next Query(), successful or not.
  [[nodiscard]] const Columns& Data() const noexcept { return data_; }

  // Value of row_title in one column of Data(). Fails if the row is missing or
  // the cell does not hold a string.
  [[nodiscard]] static std::expected<std::wstring, std::error_code>
  GetValue(const Column& column, const std::wstring& row_title) noexcept;

 private:
  // Get the index of a row by its row_title. If not found, return std::nullopt.
  [[nodiscard]] static std::optional<size_t>
  GetRowIndex(const Column& column, const std::wstring& row_title) noexcept;

  // Call a formatter on each column's row_title. A formatter manipulates the
  // row string value.
  static bool ConvertRow(Columns& data, const std::wstring& row_title,
                         const Formatter& func) noexcept;

  WmiNamespaceName wmi_namespace_;
  WmiClassName wmi_class_name_;
  Rows rows_;
  RowFormatters formatters_;

  Columns data_;
};

}  // namespace amitgdev

#endif  // AMITGDEV_WMI_NAMESPACE_HPP_