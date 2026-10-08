/*
    ValidatedString.hpp
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

#ifndef AMITGDEV_VALIDATED_STRING_HPP_
#define AMITGDEV_VALIDATED_STRING_HPP_

#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace amitgdev {

// A validator defines the validity rule for one validated-string type and
// provides a short, narrow-character description for diagnostics.
template <typename Validator>
concept StringValidator = requires(std::wstring_view text) {
  { Validator::IsValid(text) } noexcept -> std::same_as<bool>;
  { Validator::kRequirement } -> std::convertible_to<const char*>;
};

// A std::wstring whose constructed values satisfy the validator's rule.
// The invariant is preserved by copy/move construction and copy/move
// assignment because all special member operations operate only on values
// that have already satisfied the validator. Code receiving a ValidatedString
// can therefore rely on the invariant without repeating the validation.
template <StringValidator Validator>
class ValidatedString final {
 public:
  // Throws std::invalid_argument, with the validator's requirement as the
  // message, unless the text satisfies the rule. The value itself is left out
  // of the message because it is arbitrary caller text.
  explicit ValidatedString(std::wstring str) : str_(std::move(str)) {
    if (!Validator::IsValid(str_)) {
      throw std::invalid_argument(Validator::kRequirement);
    }
  }

  // Implicit from a wide-character pointer so pointer-based call sites,
  // including literals, read as plain text. A separate overload is required
  // because converting through std::wstring would require two user-defined
  // conversions. Delegation ensures pointer-based inputs are validated by the
  // same constructor.
  //
  // An array-reference constructor was considered to restrict this overload
  // to literals and avoid null pointers. It was rejected because it adds an
  // unnecessary API restriction and triggers clang-tidy's array-to-pointer
  // diagnostic when constructing the std::wstring. The pointer overload is
  // therefore retained intentionally.
  // NOLINTNEXTLINE(google-explicit-constructor)
  ValidatedString(const wchar_t* str) : ValidatedString(std::wstring{str}) {}

  // Implicit on purpose: the validated value can be passed to APIs expecting
  // a const std::wstring& without an explicit extraction step.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator const std::wstring&() const noexcept { return str_; }

  // Implicit on purpose: expose a non-owning view for APIs that consume text
  // without requiring a temporary std::wstring copy.
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator std::wstring_view() const noexcept { return str_; }

  // A non-template overload is needed because standard operator+ templates do
  // not use these conversion operators for template argument deduction.
  // This enables "prefix + value" and constructs the result at its final size.
  [[nodiscard]] friend std::wstring operator+(std::wstring_view prefix,
                                              const ValidatedString& str) {
    std::wstring result;
    result.reserve(prefix.size() + str.str_.size());
    result.append(prefix).append(str.str_);
    return result;
  }

  // Explicitly exposes the null-terminated representation required by
  // C-style and COM APIs without adding an implicit pointer conversion.
  // NOLINTNEXTLINE(readability-identifier-naming)
  [[nodiscard]] const wchar_t* c_str() const noexcept { return str_.c_str(); }

 private:
  std::wstring str_;
};

}  // namespace amitgdev

#endif  // AMITGDEV_VALIDATED_STRING_HPP_