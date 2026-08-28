// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/pipeline/pipeline_cache.hpp>

#include <libsbx/utility/assert.hpp>

namespace sbx::graphics {

// Blend list is normalized to one entry per color format (default-disabled where absent) to match
// what graphics_pipeline builds — otherwise identical pipelines could key differently.
auto _build_state_from_create_info(const graphics_pipeline::create_info& create_info) -> pipeline_state {
  auto state = pipeline_state{};

  utility::assert_that(create_info.shader != nullptr, "pipeline_cache requires a shader");

  state.shader = create_info.shader->id();
  state.color_formats = create_info.color_formats;
  state.depth_format = create_info.depth_format;
  state.topology = create_info.topology;
  state.primitive_restart = create_info.primitive_restart;
  state.polygon_mode = create_info.polygon_mode;
  state.cull_mode = create_info.cull_mode;
  state.front_face = create_info.front_face;

  if (create_info.depth_bias.has_value()) {
    state.depth_bias_enable = true;
    state.depth_bias_constant = create_info.depth_bias->constant_factor;
    state.depth_bias_slope = create_info.depth_bias->slope_factor;
    state.depth_bias_clamp = create_info.depth_bias->clamp;
  }

  state.depth_test = create_info.depth_test;
  state.depth_write = create_info.depth_write;
  state.depth_compare = create_info.depth_compare;
  state.samples = create_info.samples;

  state.color_blend_attachments.reserve(create_info.color_formats.size());

  for (auto index = std::size_t{0u}; index < create_info.color_formats.size(); ++index) {
    state.color_blend_attachments.push_back(
      (index < create_info.color_blend_attachments.size())
        ? create_info.color_blend_attachments[index]
        : blend_attachment{});
  }

  state.specialization_constants = create_info.specialization_constants;

  return state;
}

auto pipeline_state_hash::operator()(const pipeline_state& state) const noexcept -> std::size_t {
  auto seed = std::size_t{0u};

  utility::hash_combine(seed, state.shader);

  for (const auto format : state.color_formats) {
    utility::hash_combine(seed, format);
  }

  utility::hash_combine(seed, state.depth_format, state.topology, state.primitive_restart, state.polygon_mode, state.cull_mode, state.front_face);
  utility::hash_combine(seed, state.depth_bias_enable, state.depth_bias_constant, state.depth_bias_slope, state.depth_bias_clamp);
  utility::hash_combine(seed, state.depth_test, state.depth_write, state.depth_compare, state.samples);

  for (const auto& blend : state.color_blend_attachments) {
    utility::hash_combine(seed, blend.enable, blend.source_color, blend.destination_color, blend.color_operation, blend.source_alpha, blend.destination_alpha, blend.alpha_operation, blend.color_write_mask);
  }

  for (const auto& constant : state.specialization_constants) {
    utility::hash_combine(seed, constant.constant_id, constant.value);
  }

  return seed;
}

auto pipeline_cache::get(const graphics_pipeline::create_info& create_info) -> memory::observer_ptr<graphics_pipeline> {
  const auto state = _build_state_from_create_info(create_info);

  if (auto entry = _pipelines.find(state); entry != _pipelines.end()) {
    return memory::make_observer(entry->second.get());
  }

  auto [entry, _] = _pipelines.emplace(state, std::make_unique<graphics_pipeline>(create_info));

  return memory::make_observer(entry->second.get());
}

} // namespace sbx::graphics
