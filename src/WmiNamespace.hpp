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
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "WmiClassName.hpp"
#include "WmiNamespaceName.hpp"
#include "WmiWrapper.hpp"

namespace amitgdev {

// Turns the text of one property into display text. It knows nothing about WMI,
// so it can be a free function, a lambda or one of the static formatters below.
using Formatter = std::function<std::wstring(const std::wstring&)>;

// Names the property a formatter applies to.
struct RowFormatter {
  std::wstring row_title;
  Formatter format;
};

using RowFormatters = std::vector<RowFormatter>;

class WmiNamespace {
 public:
  // The formatters run after every Query(), in list order and before
  // PostQueryEvent(). A formatter for a property that is not in rows has no
  // effect.
  WmiNamespace(WmiNamespaceName wmi_namespace, WmiClassName wmi_class_name,
               Rows rows, RowFormatters formatters = {});

  WmiNamespace(WmiClassName wmi_class_name, Rows rows,
               RowFormatters formatters = {})
      : WmiNamespace(L"ROOT\\CIMV2", std::move(wmi_class_name), std::move(rows),
                     std::move(formatters)) {}

  // A polymorphic base class must have a virtual destructor.
  virtual ~WmiNamespace() = default;

  WmiNamespace(const WmiNamespace&) = delete;
  WmiNamespace& operator=(const WmiNamespace&) = delete;
  WmiNamespace(WmiNamespace&&) = delete;
  WmiNamespace& operator=(WmiNamespace&&) = delete;

  // Runs the query and returns the number of objects found. A class name that
  // is not a plain identifier is refused and yields 0.
  size_t Query();

  // The reference stays valid until the next Query().
  [[nodiscard]] const Columns& Data() const noexcept { return data_; }

  // Call a formatter on each column's row_title. A formatter manipulates the
  // row string value.
  static bool ConvertRow(Columns& data, const std::wstring& row_title,
                         const Formatter& func) noexcept;

  // Get the index of a row by its row_title. If not found, return SIZE_MAX.
  [[nodiscard]] static size_t
  GetRowIndex(const Column& column, const std::wstring& row_title) noexcept;

 protected:
  [[nodiscard]] std::wstring
  GetAt(size_t pos, const std::wstring& row_title) const noexcept;

  // Lets a derived class post-process the result in place from
  // PostQueryEvent(), which a read-only Data() could not support.
  [[nodiscard]] Columns& MutableData() noexcept { return data_; }

 private:
  WmiNamespaceName wmi_namespace_;
  WmiClassName wmi_class_name_;
  Rows rows_;
  RowFormatters formatters_;

  Columns data_;
};

}  // namespace amitgdev

#endif  // AMITGDEV_WMI_NAMESPACE_HPP_