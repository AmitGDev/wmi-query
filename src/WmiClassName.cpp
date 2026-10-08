/*
    WmiClassName.cpp
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

#include "WmiClassName.hpp"

#include <string_view>

namespace amitgdev {

bool WmiClassNameRule::IsValid(std::wstring_view text) noexcept {
  constexpr std::wstring_view kAllowed{
      L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_"};

  if (text.empty()) {
    return false;
  }

  // A leading digit is not an identifier, and a leading underscore is how WMI
  // marks system classes (such as __Namespace), which are not queried here.
  const wchar_t first = text.front();
  const bool starts_with_letter =
      (first >= L'A' && first <= L'Z') || (first >= L'a' && first <= L'z');

  return starts_with_letter &&
         text.find_first_not_of(kAllowed) == std::wstring_view::npos;
}

}  // namespace amitgdev