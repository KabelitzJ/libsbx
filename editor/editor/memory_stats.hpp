// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_MEMORY_STATS_HPP_
#define EDITOR_MEMORY_STATS_HPP_

#include <cstddef>

namespace editor::memory_stats {

/**
 * @brief Global allocation counters, updated from the operator new/delete overrides in
 * memory.cpp -- only when SBX_TRACK_MEMORY is defined (see the CMake option of the same name).
 *
 * Byte counts are the allocator's real block sizes (malloc_usable_size / _msize), read at both
 * new and delete, so current_usage is exact for every delete overload -- sized or not, aligned or
 * not. They include the allocator's rounding and the aligned overloads' padding, so they run a
 * little above the requested sizes. Only memory from operator new is counted: plain malloc (ImGui,
 * the .NET runtime, Vulkan's allocator) isn't.
 */
[[nodiscard]] auto is_tracking_enabled() noexcept -> bool;

[[nodiscard]] auto total_allocated() noexcept -> std::size_t;

[[nodiscard]] auto total_freed() noexcept -> std::size_t;

[[nodiscard]] auto current_usage() noexcept -> std::size_t;

[[nodiscard]] auto peak_usage() noexcept -> std::size_t;

[[nodiscard]] auto alloc_count() noexcept -> std::size_t;

[[nodiscard]] auto dealloc_count() noexcept -> std::size_t;

/**
 * @brief The whole process's memory as the OS sees it -- resident set size on Linux, private bytes on Windows (what Visual
 * Studio's "Process Memory" graph shows), 0 elsewhere. Independent of SBX_TRACK_MEMORY, and unlike the counters above it
 * also includes plain malloc (ImGui, the .NET runtime, driver allocations).
 */
[[nodiscard]] auto process_memory_usage() -> std::size_t;

} // namespace editor::memory_stats

#endif // EDITOR_MEMORY_STATS_HPP_
