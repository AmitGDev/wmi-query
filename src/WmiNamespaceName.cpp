/*
    WmiNamespaceName.cpp
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

#include "WmiNamespaceName.hpp"

#include <string_view>

namespace amitgdev {

static bool IsAsciiLetter(const wchar_t chr) noexcept {
  return (chr >= L'A' && chr <= L'Z') || (chr >= L'a' && chr <= L'z');
}

static bool IsAsciiDigit(const wchar_t chr) noexcept {
  return chr >= L'0' && chr <= L'9';
}

bool WmiNamespaceNameRule::IsValid(std::wstring_view text) noexcept {
  // Walks the text once, tracking whether the next character starts a segment.
  // That rejects an empty path, a leading, doubled or trailing separator, and
  // the "\\server\root" remote form (it starts with a separator) in one pass.
  bool at_segment_start = true;

  for (const wchar_t chr : text) {
    if (chr == L'\\') {
      if (at_segment_start) {
        return false;
      }
      at_segment_start = true;
    } else if (at_segment_start) {
      if (!IsAsciiLetter(chr)) {
        return false;
      }
      at_segment_start = false;
    } else if (!IsAsciiLetter(chr) && !IsAsciiDigit(chr) && chr != L'_') {
      return false;
    }
  }

  return !text.empty() && !at_segment_start;
}

}  // namespace amitgdev