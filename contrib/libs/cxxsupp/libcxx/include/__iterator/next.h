// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___ITERATOR_NEXT_H
#define _LIBCPP___ITERATOR_NEXT_H

#include <__config>
#include <__debug>
#include <__function_like.h>
#include <__iterator/advance.h>
#include <__iterator/concepts.h>
#include <__iterator/incrementable_traits.h>
#include <__iterator/iterator_traits.h>
#include <type_traits>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#pragma GCC system_header
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _InputIter>
inline _LIBCPP_INLINE_VISIBILITY _LIBCPP_CONSTEXPR_AFTER_CXX14
    typename enable_if<__is_cpp17_input_iterator<_InputIter>::value, _InputIter>::type
    next(_InputIter __x, typename iterator_traits<_InputIter>::difference_type __n = 1) {
  _LIBCPP_ASSERT(__n >= 0 || __is_cpp17_bidirectional_iterator<_InputIter>::value,
                 "Attempt to next(it, n) with negative n on a non-bidirectional iterator");

  _VSTD::advance(__x, __n);
  return __x;
}

#if !defined(_LIBCPP_HAS_NO_RANGES)

namespace ranges {
// TODO(varconst): rename `__next_fn` to `__fn`.
struct __next_fn final : private __function_like {
  _LIBCPP_HIDE_FROM_ABI
  constexpr explicit __next_fn(__tag __x) noexcept : __function_like(__x) {}

  template <input_or_output_iterator _Ip>
  _LIBCPP_HIDE_FROM_ABI
  constexpr _Ip operator()(_Ip __x) const {
    ++__x;
    return __x;
  }

  template <input_or_output_iterator _Ip>
  _LIBCPP_HIDE_FROM_ABI
  constexpr _Ip operator()(_Ip __x, iter_difference_t<_Ip> __n) const {
    ranges::advance(__x, __n);
    return __x;
  }

  template <input_or_output_iterator _Ip, sentinel_for<_Ip> _Sp>
  _LIBCPP_HIDE_FROM_ABI
  constexpr _Ip operator()(_Ip __x, _Sp ___bound) const {
    ranges::advance(__x, ___bound);
    return __x;
  }

  template <input_or_output_iterator _Ip, sentinel_for<_Ip> _Sp>
  _LIBCPP_HIDE_FROM_ABI
  constexpr _Ip operator()(_Ip __x, iter_difference_t<_Ip> __n, _Sp ___bound) const {
    ranges::advance(__x, __n, ___bound);
    return __x;
  }
};

inline constexpr auto next = __next_fn(__function_like::__tag());
} // namespace ranges

#endif // !defined(_LIBCPP_HAS_NO_RANGES)

_LIBCPP_END_NAMESPACE_STD

#endif // _LIBCPP___ITERATOR_NEXT_H
