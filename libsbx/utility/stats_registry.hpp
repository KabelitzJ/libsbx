// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_STATS_REGISTRY_HPP_
#define LIBSBX_UTILITY_STATS_REGISTRY_HPP_

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include <libsbx/units/units.hpp>

#include <libsbx/utility/timer.hpp>

namespace sbx::utility {

/** @brief One named scope's most recently recorded duration — see record_scope(). */
struct scope_stat {
  std::float_t last_milliseconds{0.0f};
  std::uint32_t sample_count{0u};
}; // struct scope_stat

/**
 * @brief A Tracy-independent, always-on CPU scope timing registry, read by the editor's
 * Statistics panel (Performance tab).
 *
 * Thread-safe: render passes record from the render thread while the editor reads on the main thread.
 *
 * @param name The scope's name; entries are keyed by this.
 * @param elapsed The scope's duration.
 */
auto record_scope(std::string_view name, const units::seconds& elapsed) -> void;

/**
 * @brief A snapshot of every recorded scope.
 *
 * @return Every scope recorded so far, keyed by name.
 */
[[nodiscard]] auto scope_stats() -> std::unordered_map<std::string, scope_stat>;

/** @brief Clears every entry; the engine loop calls it at the start of each frame. */
auto clear_scope_stats() -> void;

} // namespace sbx::utility

#define SBX_STATS_SCOPE_CONCAT_(a, b) a##b
#define SBX_STATS_SCOPE_CONCAT(a, b) SBX_STATS_SCOPE_CONCAT_(a, b)

/** @brief Always-on sibling of SBX_PROFILE_SCOPE, timing the enclosing scope into record_scope(). */
#define SBX_STATS_SCOPE(name) \
  auto SBX_STATS_SCOPE_CONCAT(_sbx_stats_scope_, __LINE__) = ::sbx::utility::scoped_timer{[](const ::sbx::units::seconds& elapsed) { ::sbx::utility::record_scope(name, elapsed); }}

#endif // LIBSBX_UTILITY_STATS_REGISTRY_HPP_
