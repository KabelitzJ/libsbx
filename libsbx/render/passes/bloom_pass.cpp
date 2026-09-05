// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/bloom_pass.hpp>

#include <algorithm>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

inline constexpr auto threads_per_group = std::uint32_t{8u};

auto bloom_pass::extent_for(const math::vector2u& color_extent) noexcept -> math::vector2u {
  return math::vector2u{std::max(color_extent.x() / 2u, 1u), std::max(color_extent.y() / 2u, 1u)};
}

auto bloom_pass::mip_count_for(const math::vector2u& color_extent) noexcept -> std::uint32_t {
  const auto base = extent_for(color_extent);

  return std::clamp(graphics::image::mip_levels_for(math::vector3u{base, 1u}), 2u, max_mip_count);
}

bloom_pass::bloom_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto prefilter_shader = shader_cache.get({"shaders/passes/bloom_prefilter.slang", entry_points});

  _prefilter_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = prefilter_shader,
    .name = "Bloom Prefilter"
  });

  const auto downsample_shader = shader_cache.get({"shaders/passes/bloom_downsample.slang", entry_points});

  _downsample_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = downsample_shader,
    .name = "Bloom Downsample"
  });

  const auto upsample_shader = shader_cache.get({"shaders/passes/bloom_upsample.slang", entry_points});

  _upsample_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = upsample_shader,
    .name = "Bloom Upsample"
  });
}

bloom_pass::~bloom_pass() {
  for (const auto& mip : _downsample_mips) {
    _destroy(mip);
  }

  for (const auto& mip : _upsample_mips) {
    _destroy(mip);
  }

  for (const auto& chain : _retired) {
    for (const auto& mip : chain.views) {
      _destroy(mip);
    }
  }
}

auto bloom_pass::_destroy(const mip_view& mip) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  bindless_table.unregister_sampled_image(mip.sampled_index);
  bindless_table.unregister_storage_image(mip.storage_index);

  vkDestroyImageView(graphics_module.logical_device(), mip.view, nullptr);
}

auto bloom_pass::_drain_retired() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& frame_context = graphics_module.frame_context();

  const auto completed_value = frame_context.timeline_value();

  while (!_retired.empty() && _retired.front().frame_index <= completed_value) {
    for (const auto& mip : _retired.front().views) {
      _destroy(mip);
    }

    _retired.pop_front();
  }
}

auto bloom_pass::_rebuild_views(const graph_resources& resources) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& frame_context = graphics_module.frame_context();

  _drain_retired();

  if (!_downsample_mips.empty() || !_upsample_mips.empty()) {
    // Frames still in flight from before this resize may still reference the old views/indices
    // (their command buffers were already recorded against them) -- retire instead of destroying.
    auto chain = retired_chain{frame_context.frame_index(), {}};
    chain.views.reserve(_downsample_mips.size() + _upsample_mips.size());

    for (auto& mip : _downsample_mips) {
      chain.views.push_back(mip);
    }

    for (auto& mip : _upsample_mips) {
      chain.views.push_back(mip);
    }

    _retired.push_back(std::move(chain));
  }

  _downsample_mips.clear();
  _upsample_mips.clear();

  _cached_downsample = resources.bloom_downsample;
  _cached_upsample = resources.bloom_upsample;

  if (!resources.bloom_downsample.is_valid() || !resources.bloom_upsample.is_valid()) {
    _mip_count = 0u;
    return;
  }

  auto& downsample_image = registry.get<graphics::image>(resources.bloom_downsample);

  _mip_count = downsample_image.mip_levels();

  _downsample_mips.resize(_mip_count);

  for (auto mip = std::uint32_t{0u}; mip < _mip_count; ++mip) {
    const auto view = downsample_image.create_view(graphics::image_view_type::two_dimensional, mip, 1u, 0u, 1u);
    const auto sampled_index = bindless_table.register_sampled_image(view);
    const auto storage_index = bindless_table.register_storage_image(view);

    _downsample_mips[mip] = mip_view{view, sampled_index, storage_index};
  }

  auto& upsample_image = registry.get<graphics::image>(resources.bloom_upsample);

  _upsample_mips.resize(_mip_count - 1u);

  for (auto mip = std::uint32_t{0u}; mip < _mip_count - 1u; ++mip) {
    const auto view = upsample_image.create_view(graphics::image_view_type::two_dimensional, mip, 1u, 0u, 1u);
    const auto sampled_index = bindless_table.register_sampled_image(view);
    const auto storage_index = bindless_table.register_storage_image(view);

    _upsample_mips[mip] = mip_view{view, sampled_index, storage_index};
  }

  bindless_table.flush_writes();
}

auto bloom_pass::declare(compute_pass_builder& builder, const graph_resources& resources) -> void {
  builder.reads_image(resources.color, graphics::pipeline_stage::compute_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);

  if (resources.bloom_downsample != _cached_downsample || resources.bloom_upsample != _cached_upsample) {
    _rebuild_views(resources);
  }

  builder.declares_image_ready(resources.bloom_upsample, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);
}

struct prefilter_push_data {
  std::uint32_t color_index;
  std::uint32_t sampler_index;
  std::uint32_t output_index;
  std::float_t threshold;
  std::float_t knee;
}; // struct prefilter_push_data

struct downsample_push_data {
  std::uint32_t source_index;
  std::uint32_t sampler_index;
  std::uint32_t output_index;
}; // struct downsample_push_data

struct upsample_push_data {
  std::uint32_t small_index;
  std::uint32_t big_index;
  std::uint32_t sampler_index;
  std::uint32_t output_index;
  std::float_t filter_radius;
}; // struct upsample_push_data

auto bloom_pass::execute(render_context& context) -> void {
  if (_mip_count == 0u) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  auto& command_buffer = *context.command_buffer;

  auto& downsample_image = registry.get<graphics::image>(_cached_downsample);
  auto& upsample_image = registry.get<graphics::image>(_cached_upsample);

  // Bloom fully regenerates its content every frame, so undefined -> general is always a valid
  // starting point -- no first-frame special case needed.
  auto to_general = graphics::command_buffer::image_transition_data{};
  to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
  to_general.src_access_mask = VK_ACCESS_2_NONE;
  to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  to_general.dst_access_mask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
  to_general.old_layout = graphics::image_layout::undefined;
  to_general.new_layout = graphics::image_layout::general;
  to_general.aspect_mask = downsample_image.aspect();

  to_general.image = downsample_image.handle();
  to_general.mip_levels = _mip_count;
  command_buffer.transition_image_layout(to_general);

  to_general.image = upsample_image.handle();
  to_general.mip_levels = _mip_count - 1u;
  command_buffer.transition_image_layout(to_general);

  const auto& camera = context.packet->camera;

  if (!camera.bloom_enabled) {
    // Never ran a dispatch, so every mip is still "general" with stale/garbage contents -- flip
    // the whole chain to the state tonemap_pass's declared read expects in one shot. tonemap_pass
    // itself zeroes the actual contribution, so the garbage never shows.
    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::general;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = downsample_image.aspect();

    to_read.image = downsample_image.handle();
    to_read.mip_levels = _mip_count;
    command_buffer.transition_image_layout(to_read);

    to_read.image = upsample_image.handle();
    to_read.mip_levels = _mip_count - 1u;
    command_buffer.transition_image_layout(to_read);

    return;
  }

  bind_compute_globals(context);

  const auto base_extent = extent_for(context.extent);

  // bindless_table's sampled-image slots are permanently declared at shader_read_only_optimal, so
  // each mip flips general -> read-only right after the dispatch that produced it, individually --
  // one whole-chain transition at the end would leave earlier mips in the wrong layout for the
  // later dispatches that sample them.
  auto mip_to_read = [&](graphics::image& image, std::uint32_t mip) -> void {
    auto transition = graphics::command_buffer::image_transition_data{};
    transition.image = image.handle();
    transition.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    transition.src_access_mask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    transition.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    transition.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    transition.old_layout = graphics::image_layout::general;
    transition.new_layout = graphics::image_layout::shader_read_only_optimal;
    transition.aspect_mask = image.aspect();
    transition.base_mip_level = mip;
    transition.mip_levels = 1u;
    command_buffer.transition_image_layout(transition);
  };

  // Stage 1: prefilter -- threshold the full-res HDR scene color straight into downsample mip0.
  {
    const auto groups_x = (base_extent.x() + threads_per_group - 1u) / threads_per_group;
    const auto groups_y = (base_extent.y() + threads_per_group - 1u) / threads_per_group;

    command_buffer.bind_pipeline(*_prefilter_pipeline);

    const auto data = prefilter_push_data{context.color_index, context.clamp_sampler_index, _downsample_mips[0].storage_index, camera.bloom_threshold, camera.bloom_knee};

    write_push_constants(context, data);

    command_buffer.dispatch(groups_x, groups_y, 1u);
  }

  mip_to_read(downsample_image, 0u);

  // Stage 2: downsample chain -- each mip reads the previous one, already flipped read-only.
  command_buffer.bind_pipeline(*_downsample_pipeline);

  for (auto mip = std::uint32_t{1u}; mip < _mip_count; ++mip) {
    const auto mip_extent = math::vector2u{std::max(base_extent.x() >> mip, 1u), std::max(base_extent.y() >> mip, 1u)};
    const auto groups_x = (mip_extent.x() + threads_per_group - 1u) / threads_per_group;
    const auto groups_y = (mip_extent.y() + threads_per_group - 1u) / threads_per_group;

    const auto data = downsample_push_data{_downsample_mips[mip - 1u].sampled_index, context.clamp_sampler_index, _downsample_mips[mip].storage_index};

    write_push_constants(context, data);

    command_buffer.dispatch(groups_x, groups_y, 1u);

    mip_to_read(downsample_image, mip);
  }

  // Stage 3: upsample chain -- tent-filters the smaller mip and additively combines with the
  // same-size downsample mip, walking back up to mip0 (bloom_upsample's coarsest level).
  command_buffer.bind_pipeline(*_upsample_pipeline);

  for (auto step = std::uint32_t{0u}; step < _mip_count - 1u; ++step) {
    const auto mip = (_mip_count - 2u) - step;

    const auto mip_extent = math::vector2u{std::max(base_extent.x() >> mip, 1u), std::max(base_extent.y() >> mip, 1u)};
    const auto groups_x = (mip_extent.x() + threads_per_group - 1u) / threads_per_group;
    const auto groups_y = (mip_extent.y() + threads_per_group - 1u) / threads_per_group;

    const auto small_index = (mip == _mip_count - 2u) ? _downsample_mips[_mip_count - 1u].sampled_index : _upsample_mips[mip + 1u].sampled_index;

    const auto data = upsample_push_data{small_index, _downsample_mips[mip].sampled_index, context.clamp_sampler_index, _upsample_mips[mip].storage_index, 1.0f};

    write_push_constants(context, data);

    command_buffer.dispatch(groups_x, groups_y, 1u);

    mip_to_read(upsample_image, mip);
  }
}

} // namespace sbx::render
