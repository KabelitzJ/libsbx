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
 * current_usage only decrements precisely for the sized operator delete overloads -- the
 * compiler prefers those whenever the static type is complete at the delete expression (true for
 * most calls), but an unsized delete (both overloads exist in memory.cpp) can't know how much to
 * subtract and is skipped, so current_usage can drift slightly high over a long session. total_
 * allocated/alloc_count/dealloc_count are always exact, since every operator new overload
 * receives count.
 */
[[nodiscard]] auto is_tracking_enabled() noexcept -> bool;

[[nodiscard]] auto total_allocated() noexcept -> std::size_t;

[[nodiscard]] auto total_freed() noexcept -> std::size_t;

[[nodiscard]] auto current_usage() noexcept -> std::size_t;

[[nodiscard]] auto peak_usage() noexcept -> std::size_t;

[[nodiscard]] auto alloc_count() noexcept -> std::size_t;

[[nodiscard]] auto dealloc_count() noexcept -> std::size_t;

} // namespace editor::memory_stats

#endif // EDITOR_MEMORY_STATS_HPP_
