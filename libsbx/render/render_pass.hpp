// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_PASS_HPP_
#define LIBSBX_RENDER_RENDER_PASS_HPP_

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/memory/observer_ptr.hpp>
#include <libsbx/memory/bytes.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/shader_graph.hpp>

#include <libsbx/render/render_packet.hpp>

namespace sbx::render {

inline constexpr auto shadow_cascade_count = std::uint32_t{4u};
inline constexpr auto shadow_map_resolution = std::uint32_t{2048u};

// frustum_cull_pass culls once per view: view 0 is the camera (opaque_commands), view 1 + c is shadow cascade c (shadow_caster_commands).
inline constexpr auto cull_view_count = std::uint32_t{1u + shadow_cascade_count};

// PCF quality tier for cascaded shadows; must match shaders/shadows/csm.slang (0 = 4 taps, 1 = 8, 2 = 16).
inline constexpr auto shadow_pcf_quality = std::uint32_t{0u};

/** @brief Per-frame state handed to every pass, filled once by the module before the pass list runs. */
struct render_context {
  memory::observer_ptr<graphics::command_buffer> command_buffer;
  memory::observer_ptr<const render_packet> packet;

  std::uint64_t frame_index{0u};
  std::uint32_t slot{0u};

  std::float_t time{0.0f};
  std::float_t delta_time{0.0f};

  math::vector2u extent{};

  math::vector2u swapchain_extent{};

  std::uint32_t environment_index{0xFFFFFFFFu};
  std::float_t environment_intensity{1.0f};
  std::float_t ambient_intensity{1.0f};
  std::uint32_t irradiance_index{0xFFFFFFFFu};
  std::uint32_t brdf_lut_index{0xFFFFFFFFu};
  std::uint32_t prefiltered_index{0xFFFFFFFFu};
  std::uint32_t prefiltered_mip_count{0u};
  math::matrix4x4 inverse_view_projection{math::matrix4x4::identity};

  graphics::image_handle depth{};
  // Single-sample depth (depth_pre_pass's resolve), shader_read_only_optimal after that pass.
  std::uint32_t scene_depth_index{0u};

  // ambient_occlusion_pass's half-resolution targets (raw -> blurred): sampled and storage indices.
  graphics::image_handle ambient_occlusion_raw{};
  graphics::image_handle ambient_occlusion{};
  std::uint32_t ambient_occlusion_raw_index{0u};
  std::uint32_t ambient_occlusion_index{0u};
  std::uint32_t ambient_occlusion_raw_storage_index{0u};
  std::uint32_t ambient_occlusion_storage_index{0u};
  graphics::image_handle color{};
  graphics::image_handle color_msaa{};
  std::uint32_t color_index{0u};

  // The tonemapped, presentable result, shader-readable before any compositor runs. Only fresh when a camera was active.
  graphics::image_handle final_image{};
  std::uint32_t final_image_index{0u};

  graphics::image_handle accumulator{};
  graphics::image_handle accumulator_msaa{};
  std::uint32_t accumulator_index{0u};
  graphics::image_handle revealage{};
  graphics::image_handle revealage_msaa{};
  std::uint32_t revealage_index{0u};

  // Half-resolution blurred bloom, shader-readable by the time tonemap_pass runs even if bloom is off.
  graphics::image_handle bloom_upsample{};
  std::uint32_t bloom_upsample_index{0u};

  // selection_outline_pass: the selected-geometry mask and the jump flood ping-pong targets.
  std::uint32_t selection_mask_index{0u};
  std::array<std::uint32_t, 2u> jump_flood_indices{};

  graphics::buffer::address_type frame_address{0u};
  graphics::buffer::address_type transform_address{0u};
  std::uint32_t instance_count{0u};
  std::uint32_t sampler_index{0u};
  std::uint32_t clamp_sampler_index{0u};

  // frustum_cull_pass output for opaque_commands: one indirect draw per command and the compacted visible transforms. Only depth_pre_pass and opaque_pass use these.
  graphics::buffer_handle culled_indirect_args_buffer{};
  graphics::buffer::address_type culled_indirect_args_address{0u};
  // This frame's slot in the shared indirect args ring buffer, in commands, not bytes.
  std::uint32_t culled_indirect_args_slot_offset{0u};
  graphics::buffer::address_type culled_transform_address{0u};
  // Per cull view strides: view v's args start v * culled_indirect_args_view_stride commands in, its transforms v * culled_transform_view_stride transforms in.
  std::uint32_t culled_indirect_args_view_stride{0u};
  std::uint32_t culled_transform_view_stride{0u};
  // This slot's region of the instanced culled pool; see instanced_transform_address.
  graphics::buffer::address_type instanced_culled_address{0u};

  // This frame's region of the joint palette, written by the CPU every frame and read by skin_pass.
  graphics::buffer::address_type joint_palette_address{0u};

  graphics::buffer::address_type cluster_aabb_address{0u};
  graphics::buffer::address_type cluster_range_address{0u};
  graphics::buffer::address_type cluster_light_index_address{0u};
  graphics::buffer::address_type cluster_counter_address{0u};

  // GPU-path particle pools, ping-ponged by frame_index % 2. draw_args is only valid after particle_simulate_pass has run.
  graphics::buffer::address_type particle_additive_particles_address{0u};
  graphics::buffer::address_type particle_additive_alive_list_address{0u};
  graphics::buffer::address_type particle_additive_emitters_address{0u};
  graphics::buffer_handle particle_additive_draw_args{};
  graphics::buffer::address_type particle_alpha_particles_address{0u};
  graphics::buffer::address_type particle_alpha_alive_list_address{0u};
  graphics::buffer::address_type particle_alpha_emitters_address{0u};
  graphics::buffer_handle particle_alpha_draw_args{};

  bool show_grid{false};

  bool wireframe{false}; // opaque_pass draws with line pipelines

  bool has_shadow_caster{false};
  std::array<graphics::image_handle, shadow_cascade_count> shadow_maps{};
  std::array<std::uint32_t, shadow_cascade_count> shadow_map_indices{};
}; // struct render_context

/**
 * @brief Where an instanced draw's visible transforms for a cull view live in the instanced culled pool; frustum_cull_pass writes them there.
 *
 * @param context The frame's render context.
 * @param command The instanced draw command.
 * @param cascade_index The shadow cascade, or 0xFFFFFFFF for the camera view.
 *
 * @return The address of the visible transforms.
 */
auto instanced_transform_address(const render_context& context, const draw_command& command, std::uint32_t cascade_index) -> graphics::buffer::address_type;

struct push_constants {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type vertex_address;
  graphics::buffer::address_type transform_address;
  std::uint32_t transform_offset;
  std::uint32_t material_index;
  std::uint32_t sampler_index;
  std::uint32_t clamp_sampler_index;
  std::uint32_t cascade_index{0xFFFFFFFFu}; // set per cascade by shadow_pass

  // Only for shader graph Time/Delta Time nodes; other shaders declare a prefix of this struct.
  std::float_t time{0.0f};
  std::float_t delta_time{0.0f};
}; // struct push_constants

static_assert(sizeof(push_constants) <= 128u, "Push constants must not exceed 128 bytes.");

/** @brief A render stage using dynamic rendering. Owns its pipelines, targets and barriers; the module owns only the swapchain transitions. */
class render_pass : public utility::noncopyable {

public:

  inline static constexpr auto hdr_format = graphics::format::r16g16b16a16_sfloat;
  inline static constexpr auto sample_count = graphics::samples::count_4;

  virtual ~render_pass() = default;

  [[nodiscard]] virtual auto name() const -> std::string_view = 0;

  virtual auto execute(render_context& context) -> void = 0;

}; // class render_pass

/**
 * @brief Resolves a shader graph or shader_code material's pipeline on demand, swapping its shader and cull mode into the pass's own pipeline template.
 *
 * Returns null when the shader has no usable pipeline (codegen or compilation failed); the caller skips the draw.
 * Every depth or color writing pass supplies one, since a custom vertex stage can displace the mesh.
 */
using custom_pipeline_resolver = std::function<memory::observer_ptr<graphics::graphics_pipeline>(const std::string& shader_path, bool is_double_sided)>;

/**
 * @brief The shader_cache path a shader graph or shader_code material renders with.
 *
 * @param material The material.
 *
 * @return The path, or empty for other materials or when nothing is assigned.
 */
[[nodiscard]] auto custom_shader_path(const assets::material& material) -> std::string;

/**
 * @brief Shared body of every pass's custom pipeline resolver: compiles the shader on first use, fills it into @p pipeline_template and fetches the pipeline from pipeline_cache.
 *
 * @param shader_path The shader to use.
 * @param entry_points The entry points to request.
 * @param pipeline_template The pass's pipeline state, already set up.
 * @param pass_label Used for the pipeline name and warnings.
 *
 * @return The pipeline, or null (after a warning) if @p shader_path is empty or fails to compile.
 */
[[nodiscard]] auto resolve_custom_pipeline(const std::string& shader_path, std::span<const graphics::shader_compiler::entry_point_request> entry_points, graphics::graphics_pipeline::create_info pipeline_template, std::string_view pass_label) -> memory::observer_ptr<graphics::graphics_pipeline>;

auto submit_draw_commands(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, std::uint32_t cascade_index = 0xFFFFFFFFu, const custom_pipeline_resolver& resolve_custom_pipeline = {}) -> void;

/**
 * @brief Like submit_draw_commands, but for commands frustum_cull_pass already culled: draws indirectly from the culled args and fetches instances from the compacted transforms.
 *
 * @param context The frame's render context.
 * @param commands The culled command list (opaque_commands, or shadow_caster_commands per cascade).
 * @param pipelines The pass's pipelines, indexed by draw_command::pipeline_id (bit 0 double-sided, bit 1 unlit).
 * @param resolve_custom_pipeline Resolver for custom-shader materials.
 * @param cascade_index The shadow cascade whose cull view to use, or 0xFFFFFFFF for the camera.
 */
auto submit_draw_commands_indirect(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, const custom_pipeline_resolver& resolve_custom_pipeline = {}, std::uint32_t cascade_index = 0xFFFFFFFFu) -> void;

auto bind_globals(render_context& context) -> void;

auto bind_globals(render_context& context, const math::vector2u& extent) -> void;

auto bind_compute_globals(render_context& context) -> void;

template<typename Type>
auto write_push_constants(render_context& context, const Type& data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, memory::as_bytes(data));
}

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_PASS_HPP_
