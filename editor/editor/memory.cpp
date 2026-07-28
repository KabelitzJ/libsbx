// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <utility>

#include <libsbx/utility/profiler.hpp>

constexpr auto tracy_depth = 10;

namespace detail {

auto aligned_malloc(std::size_t size, std::size_t alignment) -> void* {
  auto const total = size + alignment - 1 + sizeof(void*);

  auto const raw = std::malloc(total);

  if (raw == nullptr) {
    return nullptr;
  }

  auto const raw_addr = reinterpret_cast<std::uintptr_t>(raw) + sizeof(void*);
  auto const aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);
  auto const aligned = reinterpret_cast<void*>(aligned_addr);

  reinterpret_cast<void**>(aligned)[-1] = raw;

  return aligned;
}

auto aligned_free(void* ptr) -> void {
  if (ptr != nullptr) {
    std::free(reinterpret_cast<void**>(ptr)[-1]);
  }
}

} // namespace detail

auto operator new(std::size_t count) -> void* {
  auto ptr = std::malloc(count);
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count) -> void* {
  auto ptr = std::malloc(count);
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new(std::size_t count, std::align_val_t alignment) -> void* {
  auto ptr = detail::aligned_malloc(count, static_cast<std::size_t>(alignment));
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count, std::align_val_t alignment) -> void* {
  auto ptr = detail::aligned_malloc(count, static_cast<std::size_t>(alignment));
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

// nothrow new

auto operator new(std::size_t count, std::nothrow_t const&) noexcept -> void* {
  auto ptr = std::malloc(count);
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count, std::nothrow_t const&) noexcept -> void* {
  auto ptr = std::malloc(count);
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new(std::size_t count, std::align_val_t alignment, std::nothrow_t const&) noexcept -> void* {
  auto ptr = detail::aligned_malloc(count, static_cast<std::size_t>(alignment));
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count, std::align_val_t alignment, std::nothrow_t const&) noexcept -> void* {
  auto ptr = detail::aligned_malloc(count, static_cast<std::size_t>(alignment));
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

// delete

auto operator delete(void* ptr) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::size_t size) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::size_t size) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

// aligned delete

auto operator delete(void* ptr, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  detail::aligned_free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  detail::aligned_free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::size_t size, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  detail::aligned_free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::size_t size, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  detail::aligned_free(ptr);
}

// nothrow delete (called only if the matching nothrow new's ctor throws)

auto operator delete(void* ptr, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::align_val_t alignment, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  detail::aligned_free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::align_val_t alignment, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  detail::aligned_free(ptr);
}
