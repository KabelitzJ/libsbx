// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MEMORY_RAW_ALLOCATE_HPP_
#define LIBSBX_MEMORY_RAW_ALLOCATE_HPP_

#include <memory>

namespace sbx::memory {

inline auto raw_allocate(const std::size_t size, const std::align_val_t alignment = std::align_val_t{alignof(std::max_align_t)}) -> std::byte* {
  return static_cast<std::byte*>(::operator new(size, alignment));
}

template<typename Type>
auto raw_allocate() -> Type* {
  return reinterpret_cast<Type*>(raw_allocate(sizeof(Type), std::align_val_t{alignof(Type)}));
}

inline auto raw_deallocate(std::byte* pointer, const std::align_val_t alignment = std::align_val_t{alignof(std::max_align_t)}) -> void {
  ::operator delete(pointer, alignment); 
}

template<typename Type>
auto raw_deallocate(Type* pointer) -> void {
  raw_deallocate(reinterpret_cast<std::byte*>(pointer), std::align_val_t{alignof(Type)}); 
}

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_RAW_ALLOCATE_HPP_