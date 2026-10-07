/*
    Formatters.hpp
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