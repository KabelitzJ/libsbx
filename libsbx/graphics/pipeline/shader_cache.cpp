// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/shader_cache.hpp>

#include <utility>

#include <libsbx/utility/hash.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

shader_cache::shader_cache() {

}

auto shader_cache::get(const request& request) -> memory::observer_ptr<const shader> {
  // Keyed on the request path as written (e.g. "engine://shaders/passes/shadow.slang"), so the
  // cache key stays stable regardless of where the "engine://" mount happens to point.
  auto lookup = key{request.path.generic_string(), request.entry_points, request.generation};

  if (auto entry = _shaders.find(lookup); entry != _shaders.end()) {
    return memory::make_observer<const shader>(entry->second.get());
  }

  auto& filesystem_module = core::engine::get_module<filesystem::filesystem_module>();
  const auto resolved_path = filesystem_module.native_path_of(request.path);

  auto compiled = std::make_unique<shader>(resolved_path, request.entry_points, _next_id++, request.material_loader);

  auto [entry, _] = _shaders.emplace(std::move(lookup), std::move(compiled));

  return memory::make_observer<const shader>(entry->second.get());
}

auto shader_cache::key_hash::operator()(const key& key) const noexcept -> std::size_t {
  auto seed = std::size_t{0u};

  utility::hash_combine(seed, key.path, key.generation);

  for (const auto& entry_point : key.entry_points) {
    utility::hash_combine(seed, entry_point.name, static_cast<std::uint32_t>(entry_point.stage), entry_point.specialization.value_or(std::string{}));
  }

  return seed;
}

} // namespace sbx::graphics
