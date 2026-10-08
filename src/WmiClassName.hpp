// SPDX-License-Identifier: MIT
// Copyright (c) 2026, Amit Gefen

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