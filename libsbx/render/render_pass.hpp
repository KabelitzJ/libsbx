// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_PASS_HPP_
#define LIBSBX_RENDER_RENDER_PASS_HPP_

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/render_packet.hpp>

namespace sbx::render {

inline constexpr auto shadow_cascade_count = std::uint32_t{4u};
inline constexpr auto shadow_map_resolution = std::uint32_t{2048u};

// PCF quality for cascaded shadow sampling (shaders/shadows/csm.slang) — must match the
// shadow_pcf_quality tiers declared there (0 = low/4 taps, 1 = medium/8 taps, 2 = high/16 taps).
inline constexpr auto shadow_pcf_quality = std::uint32_t{2u};

/**
 * @brief Per-frame state handed to every pass. The module fills the scene bindings (addresses,
 * counts, targets) once in its prepare step before running the pass list.
 */
struct render_context {
  memory::observer_ptr<graphics::command_buffer> command_buffer;
  memory::observer_ptr<const render_packet> packet;

  std::uint64_t frame_index{0u};
  std::uint32_t slot{0u};

  math::vector2u extent{};

  math::vector2u swapchain_extent{};

  std::uint32_t environment_index{0xFFFFFFFFu};
  std::float_t environment_intensity{1.0f};
  std::uint32_t irradiance_index{0xFFFFFFFFu};
  std::uint32_t brdf_lut_index{0xFFFFFFFFu};
  std::uint32_t prefiltered_index{0xFFFFFFFFu};
  std::uint32_t prefiltered_mip_count{0u};
  math::matrix4x4 inverse_view_projection{math::matrix4x4::identity};

  graphics::image_handle depth{};
  graphics::image_handle color{};
  graphics::image_handle color_msaa{};
  std::uint32_t color_index{0u};

  // Fully tonemapped, presentable color result — produced by tonemap_pass (last in the pass list)
  // and already shader_read_only_optimal by the time any composite pass runs. Valid from frame 1,
  // but only fresh for frames where a camera was active.
  graphics::image_handle final_image{};
  std::uint32_t final_image_index{0u};

  graphics::image_handle accumulator{};
  graphics::image_handle accumulator_msaa{};
  std::uint32_t accumulator_index{0u};
  graphics::image_handle revealage{};
  graphics::image_handle revealage_msaa{};
  std::uint32_t revealage_index{0u};

  graphics::buffer::address_type frame_address{0u};
  graphics::buffer::address_type transform_address{0u};
  std::uint32_t instance_count{0u};
  std::uint32_t sampler_index{0u};
  std::uint32_t clamp_sampler_index{0u};

  graphics::buffer::address_type cluster_aabb_address{0u};
  graphics::buffer::address_type cluster_range_address{0u};
  graphics::buffer::address_type cluster_light_index_address{0u};
  graphics::buffer::address_type cluster_counter_address{0u};

  graphics::buffer::address_type particle_additive_particles_address{0u};
  graphics::buffer::address_type particle_additive_alive_list_address{0u};
  graphics::buffer::address_type particle_additive_emitters_address{0u};
  graphics::buffer_handle particle_additive_draw_args{};

  graphics::buffer::address_type particle_alpha_particles_address{0u};
  graphics::buffer::address_type particle_alpha_alive_list_address{0u};
  graphics::buffer::address_type particle_alpha_emitters_address{0u};
  graphics::buffer_handle particle_alpha_draw_args{};

  bool show_grid{false};

  bool has_shadow_caster{false};
  std::array<graphics::image_handle, shadow_cascade_count> shadow_maps{};
  std::array<std::uint32_t, shadow_cascade_count> shadow_map_indices{};
}; // struct render_context

struct push_constants {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type vertex_address;
  graphics::buffer::address_type transform_address;
  std::uint32_t transform_offset;
  std::uint32_t material_index;
  std::uint32_t sampler_index;
  std::uint32_t clamp_sampler_index;
  std::uint32_t cascade_index{0xFFFFFFFFu}; // shadow_pass overrides this per cascade; ignored otherwise.
}; // struct push_constants

static_assert(sizeof(push_constants) <= 128u, "Push constants must not exceed 128 bytes.");

/**
 * @brief A logical render stage (dynamic rendering — not a VkRenderPass). Owns its own pipelines and
 * targets, and does its own resource barriers; the module owns only the swapchain transitions.
 */
class render_pass : public utility::noncopyable {

public:

  inline static constexpr auto hdr_format = graphics::format::r16g16b16a16_sfloat;
  inline static constexpr auto sample_count = graphics::samples::count_4;

  virtual ~render_pass() = default;

  [[nodiscard]] virtual auto name() const -> std::string_view = 0;

  virtual auto execute(render_context& context) -> void = 0;

}; // class render_pass

auto submit_draw_commands(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u>& pipelines, std::uint32_t cascade_index = 0xFFFFFFFFu) -> void;

auto bind_globals(render_context& context) -> void;

auto bind_globals(render_context& context, const math::vector2u& extent) -> void;

auto bind_compute_globals(render_context& context) -> void;

template<typename Type>
auto write_push_constants(render_context& context, const Type& data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), std::addressof(data), sizeof(data));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);
}

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_PASS_HPP_
