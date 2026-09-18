// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/utility/stats_registry.hpp>

namespace sbx::utility {

static auto _scope_stats = std::unordered_map<std::string, scope_stat>{};

auto record_scope(std::string_view name, const units::seconds& elapsed) -> void {
  auto& entry = _scope_stats[std::string{name}];

  entry.last_milliseconds = static_cast<std::float_t>(elapsed) * 1000.0f;
  ++entry.sample_count;
}

auto scope_stats() -> const std::unordered_map<std::string, scope_stat>& {
  return _scope_stats;
}

auto clear_scope_stats() -> void {
  _scope_stats.clear();
}

} // namespace sbx::utility
