// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_ALIGNED_STORAGE_HPP_
#define LIBSBX_MEMORY_ALIGNED_STORAGE_HPP_

#include <array>
#include <cstddef>

namespace sbx::memory {

/**
 * @brief Size bytes of storage, aligned to Alignment — for holding an object without constructing it (e.g. a slot a container will placement-new into later).
 *
 * @tparam Size The number of bytes.
 * @tparam Alignment The required alignment.
 */
template<std::size_t Size, std::size_t Alignment>
struct aligned_storage {
  struct alignas(Alignment) type {
    std::array<std::byte, Size> data;
  }; // struct type
}; // struct aligned_storage

template<std::size_t Size, std::size_t Alignment>
using aligned_storage_t = typename aligned_storage<Size, Alignment>::type;

/** @brief Storage sized and aligned to hold one Type, unconstructed. */
template<typename Type>
struct storage_for {
  using type = aligned_storage_t<sizeof(Type), alignof(Type)>;
}; // struct storage_for

template<typename Type>
using storage_for_t = typename storage_for<Type>::type;

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_ALIGNED_STORAGE_HPP_
