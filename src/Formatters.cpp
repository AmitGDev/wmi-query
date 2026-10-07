// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026, Amit Gefen

#include "Formatters.hpp"

#include <cerrno>
#include <cstdint>
#include <cuchar>
#include <cwchar>
#include <string>
#include <string_view>

namespace {

// input_unit:
// 0 - Size in Basic Unit, 1 - Size in Kilo, 2 - Size in Mega, 3 - Size in Giga,
// 4 - Size in Tera, 5 - Size in Peta.
constexpr std::wstring_view kUnits = L" KMGTP";

}  // namespace

namespace amitgdev {

std::wstring Format1024(std::wstring input, std::uint8_t input_unit) noexcept {
  if (input_unit >= kUnits.size()) {
    return input;
  }

  const wchar_t* begin = input.c_str();
  wchar_t* end = nullptr;

  try {
    errno = 0;
    const auto value = std::wcstoull(begin, &end, 10);

    if (end == begin || *end != L'\0' || errno == ERANGE) {
      return input;
    }

    auto size = value;
    auto unit = static_cast<std::size_t>(input_unit);

    while (size / 1024 > 0 && unit + 1 < kUnits.size()) {
      size /= 1024;
      ++unit;
    }

    auto result = std::to_wstring(size);
    if (unit != 0) {
      result += kUnits.at(unit);
    }

    return result;
  } catch (...) {  // NOLINT(bugprone-empty-catch)
  }

  return input;
}

std::wstring Format1000(std::wstring input,
                        const std::uint8_t input_unit) noexcept {
  // Only plain digit strings can be scaled; anything else passes through.
  if (input.empty() ||
      input.find_first_not_of(L"0123456789") != std::wstring::npos) {
    return input;
  }

  try {
    // Leading zeros do not contribute to the decimal magnitude.
    const std::size_t first_digit = input.find_first_not_of(L'0');
    const std::wstring_view significant_digits =
        first_digit == std::wstring::npos
            ? std::wstring_view{L"0"}
            : std::wstring_view{input}.substr(first_digit);

    // Counting from the last significant digit makes a number of digits that
    // is a multiple of 3 ("800") land on the unit below, with all its digits
    // before the dot.
    const std::size_t last_digit = significant_digits.size() - 1;
    const std::size_t unit_index = last_digit / 3;
    const std::size_t integer_digits = (last_digit % 3) + 1;

    // The value is too large for the largest known unit.
    if (unit_index + input_unit >= kUnits.size()) {
      return input;
    }

    std::wstring normalized_size{significant_digits};
    normalized_size.insert(integer_digits, L".");

    // Trim only zeros: the inserted dot stops the trimming, so zeros of the
    // integer part ("100.000" -> "100.") are never lost. Then drop a dangling
    // dot.
    normalized_size.erase(normalized_size.find_last_not_of(L'0') + 1);
    if (normalized_size.back() == L'.') {
      normalized_size.pop_back();
    }

    // As in Format1024, the base unit has no letter.
    if (unit_index + input_unit > 0) {
      normalized_size += kUnits.at(unit_index + input_unit);
    }

    return normalized_size;
  } catch (...) {  // NOLINT(bugprone-empty-catch)
  }
  return input;
}

// Format WMI date input to: "DD.MM.YYYY" string. Input that is not a WMI
// datetime is returned unchanged.
std::wstring FormatDate(std::wstring date) noexcept {
  // WMI datetime starts with "YYYYMMDD"; shorter input cannot be formatted.
  constexpr size_t kMinLength = 8;

  // Every copy lives inside the try: a string copy can throw, and the
  // noexcept contract must hold even then.
  try {
    if (date.size() < kMinLength) {
      return date;
    }

    return date.substr(6, 2) + L"." + date.substr(4, 2) + L"." +
           date.substr(0, 4);
  } catch (...) {  // NOLINT(bugprone-empty-catch)
  }
  return date;
}

std::wstring FormatB(const std::wstring& bytes) {
  return Format1024(bytes, 0);
}

std::wstring FormatKB(const std::wstring& kbytes) {
  return Format1024(kbytes, 1);
}

std::wstring FormatMB(const std::wstring& mbytes) {
  return Format1024(mbytes, 2);
}

std::wstring FormatDecimal(const std::wstring& decimal) {
  return Format1000(decimal, 2);
}

}  // namespace amitgdev