/*
    WmiClassName.hpp
    Copyright (c) 2026, Amit Gefen

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

#ifndef AMITGDEV_WMI_CLASS_NAME_HPP_
#define AMITGDEV_WMI_CLASS_NAME_HPP_

#include <string_view>

#include "ValidatedString.hpp"

namespace amitgdev {

// A WMI class name is concatenated into WQL text, so it is the one caller input
// that can alter a query. An invalid name must never be representable.
struct WmiClassNameRule {
  static constexpr const char* kRequirement =
      "WMI class name must start with a letter and contain only letters, "
      "digits or underscore";

  [[nodiscard]] static bool IsValid(std::wstring_view text) noexcept;
};

using WmiClassName = ValidatedString<WmiClassNameRule>;

}  // namespace amitgdev

#endif  // AMITGDEV_WMI_CLASS_NAME_HPP_