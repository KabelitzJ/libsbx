// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MEMORY_CACHE_HPP_
#define LIBSBX_MEMORY_CACHE_HPP_

#include <new>

namespace sbx::memory {

struct cacheline {
  inline static constexpr auto size = std::hardware_constructive_interference_size;
}; // struct cacheline

template<typename Type>
struct cacheline_aligned {
  alignas(2u * cacheline::size) Type data;

  template<typename... Args>
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
