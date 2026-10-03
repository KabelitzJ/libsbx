// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_MEMORY_STATS_HPP_
#define EDITOR_MEMORY_STATS_HPP_

#include <cstddef>

namespace editor::memory_stats {

/**
 * @brief Whether the operator new/delete counters are compiled in (SBX_TRACK_MEMORY).
 *
 * The counters use the allocator's real block sizes, so current usage is exact but slightly above requested sizes. Plain malloc (ImGui, .NET, Vulkan) isn't counted.
 *
 * @return True if tracking is enabled.
 */
[[nodiscard]] auto is_tracking_enabled() noexcept -> bool;

[[nodiscard]] auto total_allocated() noexcept -> std::size_t;

[[nodiscard]] auto total_freed() noexcept -> std::size_t;

[[nodiscard]] auto current_usage() noexcept -> std::size_t;

[[nodiscard]] auto peak_usage() noexcept -> std::size_t;

[[nodiscard]] auto alloc_count() noexcept -> std::size_t;

[[nodiscard]] auto dealloc_count() noexcept -> std::size_t;

/**
 * @brief The process's memory as the OS sees it: resident set size on Linux, private bytes on Windows, 0 elsewhere. Includes plain malloc, unlike the counters.
 *
 * @return The usage in bytes.
 */
[[nodiscard]] auto process_memory_usage() -> std::size_t;

} // namespace editor::memory_stats

#endif // EDITOR_MEMORY_STATS_HPP_
