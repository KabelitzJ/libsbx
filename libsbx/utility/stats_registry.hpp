// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_STATS_REGISTRY_HPP_
#define LIBSBX_UTILITY_STATS_REGISTRY_HPP_

#include <cstdint>
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
 * Single-threaded by construction — every call site is on the main thread (the same assumption scoped_timer's own doc comment already makes), so there is no locking here.
 *
 * @param name The scope's name; entries are keyed by this.
 * @param elapsed The scope's duration.
 */
auto record_scope(std::string_view name, const units::seconds& elapsed) -> void;

/** @return Every scope recorded since the last clear_scope_stats() call, keyed by name. */
[[nodiscard]] auto scope_stats() -> const std::unordered_map<std::string, scope_stat>&;

/** @brief Clears every entry — call once per frame, before that frame's scopes run. */
auto clear_scope_stats() -> void;

} // namespace sbx::utility

#define SBX_STATS_SCOPE_CONCAT_(a, b) a##b
#define SBX_STATS_SCOPE_CONCAT(a, b) SBX_STATS_SCOPE_CONCAT_(a, b)

/** @brief Always-on sibling of SBX_PROFILE_SCOPE — see stats_registry.hpp's own doc comment for why this is a separate macro rather than a change to SBX_PROFILE_SCOPE itself. */
#define SBX_STATS_SCOPE(name) \
  auto SBX_STATS_SCOPE_CONCAT(_sbx_stats_scope_, __LINE__) = ::sbx::utility::scoped_timer{[](const ::sbx::units::seconds& elapsed) { ::sbx::utility::record_scope(name, elapsed); }}

#endif // LIBSBX_UTILITY_STATS_REGISTRY_HPP_
