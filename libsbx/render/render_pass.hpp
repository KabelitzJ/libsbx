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
  graphics::image_handle color{};
  graphics::image_handle color_msaa{};
  std::uint32_t color_index{0u};

  // Fully tonemapped, presentable result (tonemap_pass output), already shader_read_only_optimal
  // by the time any compositor runs. Valid from frame 1, but only fresh when a camera was active.
  graphics::image_handle final_image{};
  std::uint32_t final_image_index{0u};

  graphics::image_handle accumulator{};
  graphics::image_handle accumulator_msaa{};
  std::uint32_t accumulator_index{0u};
  graphics::image_handle revealage{};
  graphics::image_handle revealage_msaa{};
  std::uint32_t revealage_index{0u};

  // Half-resolution, fully-blurred bloom result (bloom_pass). Always shader_read_only_optimal by
  // the time tonemap_pass runs, even if bloom_enabled is off this frame -- see bloom_pass::execute.
  graphics::image_handle bloom_upsample{};
  std::uint32_t bloom_upsample_index{0u};

  graphics::buffer::address_type frame_address{0u};
  graphics::buffer::address_type transform_address{0u};
  std::uint32_t instance_count{0u};
  std::uint32_t sampler_index{0u};
  std::uint32_t clamp_sampler_index{0u};

  // frustum_cull_pass's output for this frame's opaque_commands: one VkDrawIndexedIndirectCommand
  // per command (culled_indirect_args_buffer, indexed by that command's position in
  // packet->opaque_commands) and the compacted, visibility-culled transforms
  // (culled_transform_address) submit_draw_commands_indirect points the vertex shader's instance
  // fetch at instead of transform_address. Only ever valid for depth_pre_pass/opaque_pass; every
  // other pass keeps reading transform_address as before.
  graphics::buffer_handle culled_indirect_args_buffer{};
  graphics::buffer::address_type culled_indirect_args_address{0u};
  // culled_indirect_args_buffer is one shared ring buffer across every frame-in-flight slot; this
  // is this frame's slot's element (VkDrawIndexedIndirectCommand-count, not byte) offset into it,
  // so submit_draw_commands_indirect's draw_indexed_indirect calls land in the right slot --
  // command_buffer::draw_indexed_indirect's own offset parameter is in the same element units.
  std::uint32_t culled_indirect_args_slot_offset{0u};
  graphics::buffer::address_type culled_transform_address{0u};

  // This frame's slot in the joint-palette buffer (CPU-written every frame from
  // packet->joint_matrices, so it's frame-in-flight multiplexed like transform_address); read by
  // skin_pass, combined with each skin_dispatch::joint_offset.
  graphics::buffer::address_type joint_palette_address{0u};

  graphics::buffer::address_type cluster_aabb_address{0u};
  graphics::buffer::address_type cluster_range_address{0u};
  graphics::buffer::address_type cluster_light_index_address{0u};
  graphics::buffer::address_type cluster_counter_address{0u};

  // GPU-path particles (libsbx/render/particles/particle_pool.hpp), keyed off frame_index % 2 for
  // the ping-pong alive_list. draw_args is only valid once particle_simulate_pass has run this
  // frame; particle_pass checks .is_valid() rather than assuming it's ready.
  graphics::buffer::address_type particle_additive_particles_address{0u};
  graphics::buffer::address_type particle_additive_alive_list_address{0u};
  graphics::buffer::address_type particle_additive_emitters_address{0u};
  graphics::buffer_handle particle_additive_draw_args{};
  graphics::buffer::address_type particle_alpha_particles_address{0u};
  graphics::buffer::address_type particle_alpha_alive_list_address{0u};
  graphics::buffer::address_type particle_alpha_emitters_address{0u};
  graphics::buffer_handle particle_alpha_draw_args{};

  bool show_grid{false};

  /** @brief opaque_pass draws context.packet->opaque_commands with polygon_mode::line pipelines instead of fill when set. See scene_renderer_module::set_wireframe_enabled. */
  bool wireframe{false};

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

  // Shader-graph Time/Delta Time nodes only (shaders/pbr/geometry_common.slang's push_data) -- the
  // built-in PBR/Unlit shader and the depth-only passes don't declare these trailing fields at all,
  // which is fine: a shader's own push_data struct only needs to be a prefix of this one (see
  // shaders/passes/depth_pre.slang, which already stops short of clamp_sampler_index/cascade_index).
  std::float_t time{0.0f};
  std::float_t delta_time{0.0f};
}; // struct push_constants

static_assert(sizeof(push_constants) <= 128u, "Push constants must not exceed 128 bytes.");

/**
 * @brief A logical render stage using dynamic rendering (not a VkRenderPass).
 *
 * Owns its own pipelines, targets, and resource barriers; the module owns only the swapchain
 * transitions.
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
 * @brief Resolves a custom-shader material's pipeline on demand -- a shader_graph's generated
 * .slang or a shader_code material's own file (see custom_shader_path), both providing the same
 * entry points. Given the pass already owns a pass-specific graphics_pipeline::create_info template
 * (formats, blend/depth state, shading_policy specialization), this just swaps in that shader and
 * cull mode. Every lookup goes through shader_cache/pipeline_cache, so the resolver itself needs no
 * cache of its own. Returns a null observer_ptr if the shader has no usable pipeline yet (codegen
 * or compilation failed) -- the caller skips that draw rather than crash or bind something wrong.
 * Every depth-writing/color-writing pass supplies one (opaque_pass, transparent_accumulate_pass,
 * depth_pre_pass, shadow_pass) -- a custom shader's vertex stage can displace the mesh, so
 * depth_pre_pass/shadow_pass need their own pipeline too (requesting depth_vertex_main/
 * depth_fragment_main instead of vertex_main/fragment_main<Policy>) for their depth/shadow output
 * to actually match the displaced color-pass geometry. Left empty (the default) only where the
 * parameter doesn't apply at all.
 */
using custom_pipeline_resolver = std::function<memory::observer_ptr<graphics::graphics_pipeline>(const std::string& shader_path, bool is_double_sided)>;

/**
 * @brief The shader a shader_graph/shader_code material renders with, as a shader_cache path --
 * empty for any other material, or one whose graph/file isn't assigned.
 */
[[nodiscard]] auto custom_shader_path(const assets::material& material) -> std::string;

/**
 * @brief Shared body behind every pass's own _resolve_custom_pipeline (opaque_pass,
 * transparent_accumulate_pass, depth_pre_pass, shadow_pass): looks up (compiling on first use) the
 * shader at @p shader_path for @p entry_points, fills it and a @p pass_label-derived name into
 * @p pipeline_template, and fetches or builds the pipeline via pipeline_cache. Every other field of
 * @p pipeline_template (formats, blend/depth state, cull mode, specialization) is the caller's own
 * pass-specific state, already set before calling this.
 *
 * Returns a null observer_ptr, after logging a @p pass_label-tagged warning, if @p shader_path is
 * empty or its shader fails to compile -- see custom_pipeline_resolver's own doc comment for why a null
 * result (skip this draw) is the right outcome rather than letting the exception escape into the
 * frame.
 */
[[nodiscard]] auto resolve_custom_pipeline(const std::string& shader_path, std::span<const graphics::shader_compiler::entry_point_request> entry_points, graphics::graphics_pipeline::create_info pipeline_template, std::string_view pass_label) -> memory::observer_ptr<graphics::graphics_pipeline>;

auto submit_draw_commands(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, std::uint32_t cascade_index = 0xFFFFFFFFu, const custom_pipeline_resolver& resolve_custom_pipeline = {}) -> void;

/**
 * @brief Same as submit_draw_commands, but for a command list frustum_cull_pass has already culled
 * (currently only context.packet->opaque_commands, from depth_pre_pass/opaque_pass): reads each
 * command's already-known index_count/index_offset from context.culled_indirect_args_buffer via
 * draw_indexed_indirect instead of drawing directly, and points the vertex shader's instance fetch
 * at context.culled_transform_address (the GPU-compacted, visible-only transforms) instead of
 * context.transform_address.
 */
auto submit_draw_commands_indirect(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, const custom_pipeline_resolver& resolve_custom_pipeline = {}) -> void;

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
