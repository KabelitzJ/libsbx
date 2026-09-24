// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_ALIGNMENT_HPP_
#define LIBSBX_MEMORY_ALIGNMENT_HPP_

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace sbx::memory {

/** @brief sizeof(Type), rounded up to alignof(Type) — the space one Type occupies in a tightly packed array. */
template<typename Type>
struct stride {
  inline static constexpr auto value = (sizeof(Type) + alignof(Type) - 1u) & ~(alignof(Type) - 1u);
}; // struct stride

template<typename Type>
constexpr auto stride_v = stride<Type>::value;

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_ALIGNMENT_HPP_
