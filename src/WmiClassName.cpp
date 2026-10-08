// SPDX-License-Identifier: MIT
// Copyright (c) 2026, Amit Gefen

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