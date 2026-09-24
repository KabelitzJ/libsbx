// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_CONCEPTS_HPP_
#define LIBSBX_MEMORY_CONCEPTS_HPP_

#include <memory>
#include <type_traits>

namespace sbx::memory {

/** @brief Whether Allocator allocates Type. */
template<typename Allocator, typename Type>
concept allocator_for = std::is_same_v<typename std::allocator_traits<Allocator>::value_type, Type>;

/** @brief Allocator, rebound to allocate Type instead of its own value_type. */
template<typename Allocator, typename Type>
struct rebound_allocator {
  using type = typename std::allocator_traits<Allocator>::rebind_alloc<Type>;
};

template<typename Allocator, typename Type>
using rebound_allocator_t = rebound_allocator<Allocator, Type>::type;

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_CONCEPTS_HPP_
