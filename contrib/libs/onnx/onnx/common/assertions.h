/*
 * SPDX-License-Identifier: Apache-2.0
 */

// ATTENTION: The code in this file is highly EXPERIMENTAL.
// Adventurous users should note that the APIs will probably change.

#pragma once

#include <stdexcept>
#include <util/generic/string.h>

namespace ONNX_NAMESPACE {

struct assert_error : public std::runtime_error {
 public:
  explicit assert_error(const TString& msg) : runtime_error(msg) {}
};

struct tensor_error : public assert_error {
 public:
  explicit tensor_error(TString msg) : assert_error(msg) {}
};

TString barf(const char* fmt, ...);

[[noreturn]] void throw_assert_error(TString&);

[[noreturn]] void throw_tensor_error(TString&);

} // namespace ONNX_NAMESPACE

#if defined(__GNUC__) || defined(__ICL) || defined(__clang__)
#define _ONNX_EXPECT(x, y) (__builtin_expect((x), (y)))
#else
#define _ONNX_EXPECT(x, y) (x)
#endif

#define ONNX_ASSERT(cond)                           \
  if (_ONNX_EXPECT(!(cond), 0)) {                   \
    TString error_msg = ::ONNX_NAMESPACE::barf( \
        "%s:%u: %s: Assertion `%s` failed.",        \
        __FILE__,                                   \
        __LINE__,                                   \
        __func__,                                   \
        #cond);                                     \
    throw_assert_error(error_msg);                  \
  }

// The following is used to prevent MSVC from passing the whole __VA_ARGS__ list
// as the first parameter value to a macro call.
#define ONNX_EXPAND(x) x

// Note: msg must be a string literal
#define _ONNX_ASSERTM(cond, msg, ...)               \
  if (_ONNX_EXPECT(!(cond), 0)) {                   \
    TString error_msg = ::ONNX_NAMESPACE::barf( \
        "%s:%u: %s: Assertion `%s` failed: " msg,   \
        __FILE__,                                   \
        __LINE__,                                   \
        __func__,                                   \
        #cond,                                      \
        __VA_ARGS__);                               \
    throw_assert_error(error_msg);                  \
  }

// The trailing ' ' argument is a hack to deal with the extra comma when ... is empty.
// Another way to solve this is ##__VA_ARGS__ in _ONNX_ASSERTM, but this is a non-portable
// extension we shouldn't use.
#define ONNX_ASSERTM(...) ONNX_EXPAND(_ONNX_ASSERTM(__VA_ARGS__, " "))

#define _TENSOR_ASSERTM(cond, msg, ...)             \
  if (_ONNX_EXPECT(!(cond), 0)) {                   \
    TString error_msg = ::ONNX_NAMESPACE::barf( \
        "%s:%u: %s: Assertion `%s` failed: " msg,   \
        __FILE__,                                   \
        __LINE__,                                   \
        __func__,                                   \
        #cond,                                      \
        __VA_ARGS__);                               \
    throw_tensor_error(error_msg);                  \
  }

#define TENSOR_ASSERTM(...) ONNX_EXPAND(_TENSOR_ASSERTM(__VA_ARGS__, " "))
