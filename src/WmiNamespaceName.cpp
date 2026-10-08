// SPDX-License-Identifier: MIT
// Copyright (c) 2026, Amit Gefen

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