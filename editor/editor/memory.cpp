// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#if defined(SBX_ENABLE_PROFILING) || defined(SBX_TRACK_MEMORY)

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <utility>

#include <libsbx/utility/profiler.hpp>

#include <editor/memory_stats.hpp>

constexpr auto tracy_depth = 10;

#if defined(SBX_ENABLE_PROFILING)
#define SBX_MEMORY_TRACY_ALLOC(ptr, count) TracyAllocS(ptr, count, tracy_depth)
#define SBX_MEMORY_TRACY_FREE(ptr) TracyFreeS(ptr, tracy_depth)
#else
#define SBX_MEMORY_TRACY_ALLOC(ptr, count) static_cast<void>(0)
#define SBX_MEMORY_TRACY_FREE(ptr) static_cast<void>(0)
#endif // SBX_ENABLE_PROFILING

#if defined(SBX_TRACK_MEMORY)

#include <atomic>

namespace editor::memory_stats::detail {

std::atomic<std::size_t> total_allocated{0u};
std::atomic<std::size_t> total_freed{0u};
std::atomic<std::size_t> current_usage{0u};
std::atomic<std::size_t> peak_usage{0u};
std::atomic<std::size_t> alloc_count{0u};
std::atomic<std::size_t> dealloc_count{0u};

inline auto on_alloc(std::size_t count) -> void {
  total_allocated.fetch_add(count, std::memory_order_relaxed);
  alloc_count.fetch_add(1u, std::memory_order_relaxed);

  const auto usage = current_usage.fetch_add(count, std::memory_order_relaxed) + count;

  auto previous_peak = peak_usage.load(std::memory_order_relaxed);

  while (usage > previous_peak && !peak_usage.compare_exchange_weak(previous_peak, usage, std::memory_order_relaxed)) { }
}

// Only the sized operator delete overloads can adjust current_usage -- see memory_stats.hpp's own
// doc comment on why the unsized ones are skipped rather than guessed at.
inline auto on_dealloc_sized(std::size_t size) -> void {
  total_freed.fetch_add(size, std::memory_order_relaxed);
  dealloc_count.fetch_add(1u, std::memory_order_relaxed);
  current_usage.fetch_sub(size, std::memory_order_relaxed);
}

inline auto on_dealloc_unsized() -> void {
  dealloc_count.fetch_add(1u, std::memory_order_relaxed);
}

} // namespace editor::memory_stats::detail

#define SBX_MEMORY_STATS_ALLOC(count) ::editor::memory_stats::detail::on_alloc(count)
#define SBX_MEMORY_STATS_DEALLOC_SIZED(size) ::editor::memory_stats::detail::on_dealloc_sized(size)
#define SBX_MEMORY_STATS_DEALLOC_UNSIZED() ::editor::memory_stats::detail::on_dealloc_unsized()

#else

#define SBX_MEMORY_STATS_ALLOC(count) static_cast<void>(0)
#define SBX_MEMORY_STATS_DEALLOC_SIZED(size) static_cast<void>(0)
#define SBX_MEMORY_STATS_DEALLOC_UNSIZED() static_cast<void>(0)

#endif // SBX_TRACK_MEMORY

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
  SBX_MEMORY_TRACY_ALLOC(ptr, count);
  SBX_MEMORY_STATS_ALLOC(count);
  return ptr;
}

auto operator new[](std::size_t count) -> void* {
  auto ptr = std::malloc(count);
  if (!ptr) {
    throw std::bad_alloc{};
  }
  SBX_MEMORY_TRACY_ALLOC(ptr, count);
  SBX_MEMORY_STATS_ALLOC(count);
  return ptr;
}

auto operator new(std::size_t count, std::align_val_t alignment) -> void* {
  auto ptr = detail::aligned_malloc(count, static_cast<std::size_t>(alignment));
  if (!ptr) {
    throw std::bad_alloc{};
  }
  SBX_MEMORY_TRACY_ALLOC(ptr, count);
  SBX_MEMORY_STATS_ALLOC(count);
  return ptr;
}

auto operator new[](std::size_t count, std::align_val_t alignment) -> void* {
  auto ptr = detail::aligned_malloc(count, static_cast<std::size_t>(alignment));
  if (!ptr) {
    throw std::bad_alloc{};
  }
  SBX_MEMORY_TRACY_ALLOC(ptr, count);
  SBX_MEMORY_STATS_ALLOC(count);
  return ptr;
}

// nothrow new

auto operator new(std::size_t count, std::nothrow_t const&) noexcept -> void* {
  auto ptr = std::malloc(count);
  SBX_MEMORY_TRACY_ALLOC(ptr, count);
  SBX_MEMORY_STATS_ALLOC(count);
  return ptr;
}

auto operator new[](std::size_t count, std::nothrow_t const&) noexcept -> void* {
  auto ptr = std::malloc(count);
  SBX_MEMORY_TRACY_ALLOC(ptr, count);
  SBX_MEMORY_STATS_ALLOC(count);
  return ptr;
}

auto operator new(std::size_t count, std::align_val_t alignment, std::nothrow_t const&) noexcept -> void* {
  auto ptr = detail::aligned_malloc(count, static_cast<std::size_t>(alignment));
  SBX_MEMORY_TRACY_ALLOC(ptr, count);
  SBX_MEMORY_STATS_ALLOC(count);
  return ptr;
}

auto operator new[](std::size_t count, std::align_val_t alignment, std::nothrow_t const&) noexcept -> void* {
  auto ptr = detail::aligned_malloc(count, static_cast<std::size_t>(alignment));
  SBX_MEMORY_TRACY_ALLOC(ptr, count);
  SBX_MEMORY_STATS_ALLOC(count);
  return ptr;
}

// delete

auto operator delete(void* ptr) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_UNSIZED();
  std::free(ptr);
}

auto operator delete[](void* ptr) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_UNSIZED();
  std::free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::size_t size) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_SIZED(size);
  std::free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::size_t size) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_SIZED(size);
  std::free(ptr);
}

// aligned delete

auto operator delete(void* ptr, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_UNSIZED();
  detail::aligned_free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_UNSIZED();
  detail::aligned_free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::size_t size, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_SIZED(size);
  detail::aligned_free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::size_t size, [[maybe_unused]] std::align_val_t alignment) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_SIZED(size);
  detail::aligned_free(ptr);
}

// nothrow delete (called only if the matching nothrow new's ctor throws)

auto operator delete(void* ptr, std::nothrow_t const&) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_UNSIZED();
  std::free(ptr);
}

auto operator delete[](void* ptr, std::nothrow_t const&) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_UNSIZED();
  std::free(ptr);
}

auto operator delete(void* ptr, [[maybe_unused]] std::align_val_t alignment, std::nothrow_t const&) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_UNSIZED();
  detail::aligned_free(ptr);
}

auto operator delete[](void* ptr, [[maybe_unused]] std::align_val_t alignment, std::nothrow_t const&) noexcept -> void {
  SBX_MEMORY_TRACY_FREE(ptr);
  SBX_MEMORY_STATS_DEALLOC_UNSIZED();
  detail::aligned_free(ptr);
}

#endif // SBX_ENABLE_PROFILING || SBX_TRACK_MEMORY

// Always defined, regardless of either flag, so callers (e.g. statistics_panel.cpp) can link
// unconditionally -- each simply reports zero/disabled when SBX_TRACK_MEMORY is off.
namespace editor::memory_stats {

auto is_tracking_enabled() noexcept -> bool {
#if defined(SBX_TRACK_MEMORY)
  return true;
#else
  return false;
#endif
}

auto total_allocated() noexcept -> std::size_t {
#if defined(SBX_TRACK_MEMORY)
  return detail::total_allocated.load(std::memory_order_relaxed);
#else
  return 0u;
#endif
}

auto total_freed() noexcept -> std::size_t {
#if defined(SBX_TRACK_MEMORY)
  return detail::total_freed.load(std::memory_order_relaxed);
#else
  return 0u;
#endif
}

auto current_usage() noexcept -> std::size_t {
#if defined(SBX_TRACK_MEMORY)
  return detail::current_usage.load(std::memory_order_relaxed);
#else
  return 0u;
#endif
}

auto peak_usage() noexcept -> std::size_t {
#if defined(SBX_TRACK_MEMORY)
  return detail::peak_usage.load(std::memory_order_relaxed);
#else
  return 0u;
#endif
}

auto alloc_count() noexcept -> std::size_t {
#if defined(SBX_TRACK_MEMORY)
  return detail::alloc_count.load(std::memory_order_relaxed);
#else
  return 0u;
#endif
}

auto dealloc_count() noexcept -> std::size_t {
#if defined(SBX_TRACK_MEMORY)
  return detail::dealloc_count.load(std::memory_order_relaxed);
#else
  return 0u;
#endif
}

} // namespace editor::memory_stats
