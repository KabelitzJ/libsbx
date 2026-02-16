// SPDX-License-Identifier: MIT
#include <libsbx/memory/tracking_allocator.hpp>

namespace sbx::memory {

thread_local allocation_scope::context allocation_scope::_current{allocation_category::unknown, "unknown", 0u};

} // namespace sbx::memory

inline thread_local auto is_inside_allocation = false;

auto operator new(std::size_t size) -> void* {
  if (!sbx::memory::is_memory_tracking_enabled_v || is_inside_allocation) {
    if (auto* ptr = std::malloc(size)) {
      return ptr;
    }

    throw std::bad_alloc{};
  }

  is_inside_allocation = true;

  const auto& context = sbx::memory::allocation_scope::current();

  try {
    auto* ptr = sbx::memory::detail::aligned_allocate( size, alignof(std::max_align_t), context.category, context.file_name, context.line);

    is_inside_allocation = false;

    return ptr;
  } catch (...) {
    is_inside_allocation = false;

    throw;
  }
}

auto operator new[](std::size_t size) -> void* {
  return ::operator new(size);
}


auto operator new(std::size_t size, std::align_val_t align) -> void* {
  return ::operator new(size);
}

auto operator delete(void* ptr) noexcept -> void {
  if (!ptr) {
    return;
  }

  if (!sbx::memory::is_memory_tracking_enabled_v || is_inside_allocation) {
    std::free(ptr);

    return;
  }

  is_inside_allocation = true;

  sbx::memory::detail::aligned_deallocate(ptr);

  is_inside_allocation = false;
}

auto operator delete[](void* ptr) noexcept -> void {
  ::operator delete(ptr);
}

auto operator delete(void* ptr, std::align_val_t align) noexcept -> void {
  ::operator delete(ptr);
}