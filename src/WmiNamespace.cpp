/*
    WmiNamespace.cpp
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

#include "WmiNamespace.hpp"

#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <functional>
#include <string>
#include <utility>
#include <variant>

#include "WmiClassName.hpp"
#include "WmiClient.hpp"
#include "WmiNamespaceName.hpp"
#include "WmiWrapper.hpp"

namespace amitgdev {

// By-value parameters are moved into the members, so callers can pass
// temporaries without an extra copy.
WmiNamespace::WmiNamespace(WmiNamespaceName wmi_namespace,
                           WmiClassName wmi_class_name, Rows rows,
                           RowFormatters formatters)
    : wmi_namespace_{std::move(wmi_namespace)},
      wmi_class_name_(std::move(wmi_class_name)),
      rows_(std::move(rows)),
      formatters_(std::move(formatters)) {}

// Send WMI "SELECT *" query utilizing WmiClient.
// The wmi_class_name_ & rows_ are dictated by the derived class.
// After the query, send ApplyFormatters.
size_t WmiNamespace::Query() {
  data_ = WmiClient(wmi_namespace_)
              .Query(L"SELECT * FROM " + wmi_class_name_, rows_);

  for (const auto& [row_title, format] : formatters_) {
    ConvertRow(data_, row_title, format);
  }

  return data_.size();
}

// Call a formatter on each column's row_title. A cell that does not hold a
// string, or whose formatter fails, is skipped and keeps its original value.
bool WmiNamespace::ConvertRow(Columns& data, const std::wstring& row_title,
                              const Formatter& func) noexcept {
  bool touched{false};

  if (data.empty()) {
    return touched;
  }

  // All columns have the same rows, so column 0 is enough to locate the row.
  const auto row_index = GetRowIndex(data.front(), row_title);
  if (row_index == SIZE_MAX) {
    return touched;
  }

  for (auto& column : data) {
    try {
      auto& cell = column.at(row_index).second;

      const auto* text = std::get_if<std::wstring>(&cell);
      if (text == nullptr) {
        continue;
      }

      // The formatter result is computed before the assignment, so it may
      // safely read the string it is about to replace.
      cell = func(*text);
      touched = true;
    } catch (...) {
      continue;  // Formatter or allocation failure; keep the original value.
    }
  }

  return touched;
}

// Get the value from the table at column / row_title. A missing cell, or one
// that does not hold a string, yields an empty string rather than an error,
// because callers use this for display.
std::wstring WmiNamespace::GetAt(const size_t pos,
                                 const std::wstring& row_title) const noexcept {
  try {
    if (pos >= data_.size()) {
      return {};
    }

    const Column& cells = data_.at(pos);

    const size_t row_index = GetRowIndex(cells, row_title);
    if (row_index == SIZE_MAX) {
      return {};
    }

    const auto* text = std::get_if<std::wstring>(&cells.at(row_index).second);
    if (text == nullptr) {
      return {};
    }

    return *text;
  } catch (...) {
    // noexcept contract: allocation failure degrades to "no value".
    return {};
  }
}

// Get the index of a row by its row_title. If not found, return SIZE_MAX.
size_t WmiNamespace::GetRowIndex(const Column& column,
                                 const std::wstring& row_title) noexcept {
  size_t index{0};

  for (const auto& [value_name, value_data] : column) {
    if (value_name == row_title) {
      return index;
    }

    ++index;
  }

  return SIZE_MAX;
}

}  // namespace amitgdev