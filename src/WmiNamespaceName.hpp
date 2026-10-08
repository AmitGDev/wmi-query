// SPDX-License-Identifier: MIT
// Copyright (c) 2026, Amit Gefen

#ifndef AMITGDEV_WMI_NAMESPACE_NAME_HPP_
#define AMITGDEV_WMI_NAMESPACE_NAME_HPP_

#include <string_view>

#include "ValidatedString.hpp"

namespace amitgdev {

// The name goes to ConnectServer, so it decides which namespace a query
// reaches. Only local, backslash-separated names are accepted, so a malformed
// or remote form (such as \\server\root) must never be representable.
struct WmiNamespaceNameRule {
  static constexpr const char* kRequirement =
      "WMI namespace name must be backslash-separated names, each starting "
      "with a letter and containing only letters, digits or underscore";

  [[nodiscard]] static bool IsValid(std::wstring_view text) noexcept;
};

using WmiNamespaceName = ValidatedString<WmiNamespaceNameRule>;

}  // namespace amitgdev

#endif  // AMITGDEV_WMI_NAMESPACE_NAME_HPP_