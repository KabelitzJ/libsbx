// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/compute_pipeline_cache.hpp>

#include <libsbx/utility/assert.hpp>

namespace sbx::graphics {

auto compute_pipeline_cache::get(const compute_pipeline::create_info& create_info) -> memory::observer_ptr<compute_pipeline> {
  utility::assert_that(create_info.shader != nullptr, "compute_pipeline_cache requires a shader");

  const auto id = create_info.shader->id();

  if (auto entry = _pipelines.find(id); entry != _pipelines.end()) {
    return memory::make_observer(entry->second.get());
  }

  auto [entry, _] = _pipelines.emplace(id, std::make_unique<compute_pipeline>(create_info));

  return memory::make_observer(entry->second.get());
}

} // namespace sbx::graphics
