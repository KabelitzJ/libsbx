// SPDX-License-Identifier: MIT
#include <libsbx/utility/memory_tracker.hpp>

namespace sbx::utility {

} // namespace sbx::utility

auto operator new(std::size_t size) -> void* {
  auto ptr = std::malloc(size);
  sbx::utility::memory_tracker::instance().record_allocation(ptr, size);
  return ptr;
}

auto operator new[](std::size_t size) -> void* {
  auto ptr = std::malloc(size);
  sbx::utility::memory_tracker::instance().record_allocation(ptr, size);
  return ptr;
}

auto operator delete(void* ptr) noexcept -> void {
  sbx::utility::memory_tracker::instance().record_deallocation(ptr);
  std::free(ptr);
}

auto operator delete(void* ptr, std::size_t) noexcept -> void {
  sbx::utility::memory_tracker::instance().record_deallocation(ptr);
  std::free(ptr);
}

auto operator delete[](void* ptr) noexcept -> void {
  sbx::utility::memory_tracker::instance().record_deallocation(ptr);
  std::free(ptr);
}

auto operator delete[](void* ptr, std::size_t) noexcept -> void {
  sbx::utility::memory_tracker::instance().record_deallocation(ptr);
  std::free(ptr);
}
