// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/shader_cache.hpp>

#include <utility>

#include <libsbx/utility/hash.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

shader_cache::shader_cache() { 

}

auto shader_cache::get(const request& request) -> memory::observer_ptr<const shader> {
  auto lookup = key{request.path.generic_string(), request.entry_points};

  if (auto entry = _shaders.find(lookup); entry != _shaders.end()) {
    return memory::make_observer<const shader>(entry->second.get());
  }

  auto compiled = std::make_unique<shader>(request.path, request.entry_points, _next_id++);

  auto [entry, _] = _shaders.emplace(std::move(lookup), std::move(compiled));

  return memory::make_observer<const shader>(entry->second.get());
}

auto shader_cache::key_hash::operator()(const key& key) const noexcept -> std::size_t {
  auto seed = std::size_t{0u};

  utility::hash_combine(seed, key.path);

  for (const auto& entry_point : key.entry_points) {
    utility::hash_combine(seed, entry_point.name, static_cast<std::uint32_t>(entry_point.stage), entry_point.specialization.value_or(std::string{}));
  }

  return seed;
}

} // namespace sbx::graphics
