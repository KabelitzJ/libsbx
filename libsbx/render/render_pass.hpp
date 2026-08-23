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

/**
 * @brief Per-frame state handed to every pass. The module fills the scene bindings (addresses,
 * counts, targets) once in its prepare step before running the pass list.
 */
struct render_context {
  memory::observer_ptr<graphics::command_buffer> command_buffer;
  memory::observer_ptr<const render_packet> packet;

  std::uint64_t frame_index{0u};
  std::uint32_t slot{0u};

  // Scene/offscreen render extent — what the 3D passes render into and what the projection's aspect
  // ratio is computed from. For the editor this is the Viewport panel's size, not the OS window's.
  math::vector2u extent{};

  // Always the real swapchain size, for whichever pass composites onto the actual window surface —
  // that always covers the full window/dockspace regardless of the 3D scene's own resolution.
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
  graphics::image_handle scene{};
  std::uint32_t scene_index{0u};  

  graphics::buffer::address_type frame_address{0u};
  graphics::buffer::address_type transform_address{0u};
  std::uint32_t instance_count{0u};
  std::uint32_t sampler_index{0u};
  std::uint32_t clamp_sampler_index{0u};

  // Clustered Forward+ (see light_culling_pass): this frame's slot of each cluster buffer.
  // cluster_range_address/cluster_light_index_address are also copied into frame_data itself,
  // since geometry.slang's fragment shader reads those two; cluster_aabb_address and
  // cluster_counter_address are only ever needed by light_culling_pass's own compute dispatches.
  graphics::buffer::address_type cluster_aabb_address{0u};
  graphics::buffer::address_type cluster_range_address{0u};
  graphics::buffer::address_type cluster_light_index_address{0u};
  graphics::buffer::address_type cluster_counter_address{0u};

  // Editor-only reference grid — see render_module::set_grid_enabled. Always false in demo.
  bool show_grid{false};
}; // struct render_context

struct push_constants {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type vertex_address;
  graphics::buffer::address_type transform_address;
  std::uint32_t transform_offset;
  std::uint32_t material_index;
  std::uint32_t sampler_index;
  std::uint32_t clamp_sampler_index;
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

/**
 * @brief Walks a coalesced draw list: binds the pipeline on pipeline_id change and the index buffer
 * on mesh change, pushes per-draw constants, and issues instanced draws. Residency-gates each
 * command. Assumes the bindless descriptor set + viewport/scissor are already bound.
 */
auto submit_draw_commands(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u>& pipelines) -> void;

auto bind_globals(render_context& context) -> void;

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_PASS_HPP_
