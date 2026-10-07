// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026, Amit Gefen

#include <cstdint>
#include <string>

namespace amitgdev {

// ** Common formatters (used by the Specialized Formatters): **

// Common /1024 formatter.
std::wstring Format1024(std::wstring input, std::uint8_t input_unit) noexcept;

// Common /1000 formatter.
std::wstring Format1000(std::wstring input, std::uint8_t input_unit) noexcept;

// ** Specialized Formatters (to use with ConvertRow): **

// A formatter is a plain function from text to text, independent of WMI.
// Pass by value intentionally: preserve the original value for a lossless
// fallback if anything goes wrong.

// Date:
std::wstring FormatDate(std::wstring date) noexcept;

// For binary n^2 units:
std::wstring FormatB(const std::wstring& bytes);
std::wstring FormatKB(const std::wstring& kbytes);
std::wstring FormatMB(const std::wstring& mbytes);

// For decimal x1000 units:
std::wstring FormatDecimal(const std::wstring& decimal);

}  // namespace amitgdev