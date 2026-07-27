// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <libsbx/utility/profiler.hpp>

constexpr auto tracy_depth = 10;

auto _aligned_malloc(std::size_t count, std::size_t alignment) -> void* {
  auto rounded = (count + alignment - 1) & ~(alignment - 1);

  return std::aligned_alloc(alignment, rounded);
}

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
  auto ptr = _aligned_malloc(count, static_cast<std::size_t>(alignment));
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count, std::align_val_t alignment) -> void* {
  auto ptr = _aligned_malloc(count, static_cast<std::size_t>(alignment));
  if (!ptr) {
    throw std::bad_alloc{};
  }
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

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
  auto ptr = _aligned_malloc(count, static_cast<std::size_t>(alignment));
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

auto operator new[](std::size_t count, std::align_val_t alignment, std::nothrow_t const&) noexcept -> void* {
  auto ptr = _aligned_malloc(count, static_cast<std::size_t>(alignment));
  TracyAllocS(ptr, count, tracy_depth);
  return ptr;
}

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

auto operator delete(void* ptr, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::size_t size, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::size_t size, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}

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
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::align_val_t alignment, std::nothrow_t const&) noexcept -> void {
  TracyFreeS(ptr, tracy_depth);
  std::free(ptr);
}
