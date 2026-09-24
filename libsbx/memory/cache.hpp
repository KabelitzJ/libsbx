// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_CACHE_HPP_
#define LIBSBX_MEMORY_CACHE_HPP_

#include <new>
#include <type_traits>
#include <utility>

namespace sbx::memory {

/** @brief The minimum recommended offset between two objects accessed by different threads, to avoid false sharing. */
struct cacheline {
  inline static constexpr auto size = std::hardware_destructive_interference_size;
}; // struct cacheline

/**
 * @brief Pads Type to its own cache line, so concurrently-accessed instances (e.g. adjacent
 * elements of an array of per-thread counters) don't false-share a cache line.
 *
 * @tparam Type The wrapped type.
 *
 * @param args Forwarded to Type's constructor.
 */
template<typename Type>
struct cacheline_aligned {

  alignas(cacheline::size) Type data;

  template<typename... Args>
  requires (std::is_constructible_v<Type, Args...>)
  cacheline_aligned(Args&&... args)
  : data{std::forward<Args>(args)...} { }

  operator Type&() {
    return data;
  }

  operator const Type&() const {
    return data;
  }

}; // struct cacheline_aligned

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_CACHE_HPP_
