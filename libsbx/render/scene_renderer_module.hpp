// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SCENE_RENDERER_MODULE_HPP_
#define LIBSBX_RENDER_SCENE_RENDERER_MODULE_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/devices/swapchain.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/render_packet.hpp>
#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>
#include <libsbx/render/presentation_module.hpp>
#include <libsbx/render/scene_renderer.hpp>
#include <libsbx/render/compositor.hpp>
#include <libsbx/render/debug/debug_draw.hpp>
#include <libsbx/render/particles/particle_pool.hpp>

namespace sbx::render {

/**
 * @brief Renders the active scene's 3D content into an offscreen final_image every frame.
 *
 * Knows nothing about the swapchain or ImGui; registers itself as presentation_module's
 * scene_renderer and, via scene_blit_compositor, as its default compositor.
 *
 * prepare() (main thread) extracts the active scene into a render_packet; record() — which may
 * run on a dedicated render thread — never touches the ECS.
 */
class scene_renderer_module final : public utility::noncopyable, public scene_renderer {

public:

  using dependencies = core::dependency_list<graphics::graphics_module, assets::assets_module, scenes::scenes_module, presentation_module>;

  scene_renderer_module();

  ~scene_renderer_module();

  auto prepare() -> void override;

  auto record(graphics::command_buffer& command_buffer, math::vector2u extent) -> void override;

  /**
   * @brief The extent the scene/offscreen render targets (and the projection's aspect ratio) should
   * use, overriding the default of the swapchain's extent — e.g. the editor's Viewport panel size,
   * which can differ from the OS window. Pass {0, 0} (the default) to fall back to the swapchain
   * extent; runtime never calls this, so it always renders at window resolution as before.
   */
  auto set_viewport_extent(math::vector2u extent) -> void;

  /**
   * @brief Overrides the camera_data _build_packet() would otherwise derive from the scene's
   * active camera — e.g. the editor's own fly-camera while its play_state is "edit". Pass
   * std::nullopt (the default) to fall back to the scene's active camera; runtime never calls
   * this, so it always renders through the scene's own camera exactly as before. Environment/
   * skybox is unaffected by this either way — it's always read from the scene's active camera
   * (if any) regardless of which camera_data is actually rendered with.
   */
  auto set_camera_override(std::optional<camera_data> override) -> void {
    _camera_override = override;
  }

  /**
   * @brief The final viewport image — the fully tonemapped, presentable color result (see
   * render_context::final_image). Valid from the first frame onward; the editor samples this to
   * display the scene inside its Viewport panel instead of presenting it directly.
   */
  [[nodiscard]] auto final_image() const noexcept -> graphics::image_handle {
    return _final_image;
  }

  /** @brief final_image's bindless sampled-image index — for a compositor sampling it directly (see scene_blit_compositor). */
  [[nodiscard]] auto final_image_index() const noexcept -> std::uint32_t {
    return _final_image_index;
  }

  /** @brief The general-purpose material sampler's bindless index — same one final_image itself should be sampled with. */
  [[nodiscard]] auto sampler_index() const noexcept -> std::uint32_t {
    return _sampler_index;
  }

  /**
   * @brief Whether the most recent record() actually rendered something (an active camera was
   * present) — final_image may be stale or never written otherwise. scene_blit_compositor checks
   * this instead of sampling final_image unconditionally.
   */
  [[nodiscard]] auto has_rendered() const noexcept -> bool {
    return _has_rendered;
  }

  /**
   * @brief Shows/hides the world-space reference grid (see grid_pass). Off by default; runtime
   * never calls this, so the grid pass — always present in the fixed pass list — stays a no-op
   * there. editor_module calls this once to turn it on.
   */
  auto set_grid_enabled(bool enabled) -> void;

  auto grid_enabled() const -> bool;

  /**
   * @brief The shared immediate-mode line accumulator -- physics colliders (see
   * physics::physics_module::late_update()) and, later, script-driven gizmos submit into this every
   * frame; debug_draw_pass uploads and draws whatever's accumulated, then clears it.
   */
  [[nodiscard]] auto debug_draw() noexcept -> render::debug_draw& {
    return _debug_draw;
  }

  /**
   * @brief Immediately discards every live GPU-path particle (assets::particle_simulation_mode
   * ::gpu) in both pools -- a hard reset, unlike particle_pool::tick()'s normal lifetime-based
   * drain. editor_module's play_mode_controller calls this on Stop so GPU particles don't linger
   * after CPU-mode ones are cleared by particles_module's own stopped-cleanup in the same frame.
   */
  auto reset_particles() -> void;

private:

  inline static constexpr auto light_capacity = std::uint32_t{256u};
  inline static constexpr auto transform_capacity = std::uint32_t{16384u};
  inline static constexpr auto cluster_light_index_capacity = std::uint32_t{65536u};

  // Skinning: joint_palette_capacity is a total across every skinned instance drawn this frame
  // (packet.joint_matrices), not per-instance; skin_scratch_vertex_capacity likewise sums every
  // skinned instance's vertex_count. Both are fixed upper bounds for v1 -- a frame exceeding either
  // just skips drawing the overflow (see _build_packet), no dynamic regrow.
  inline static constexpr auto joint_palette_capacity = std::uint32_t{4096u};
  inline static constexpr auto skin_scratch_vertex_capacity = std::uint32_t{65536u};

  auto _ensure_resources() -> void;

  auto _resize_targets(const math::vector2u extent) -> void;

  [[nodiscard]] auto _build_graph_resources() const -> graph_resources;

  auto _prepare_frame(render_context& context) -> void;

  [[nodiscard]] auto _build_packet() -> render_packet;

  // GPU-path only (assets::particle_simulation_mode::gpu) -- claims/keeps-alive this
  // emitter's particle_pool slot and appends a particle_emitter_snapshot to packet.particle_emitters
  // if it's currently playing. See scenes::particle_emitter::slot's doc comment for why this
  // bookkeeping lives here (render cadence) rather than particles_module (fixed-step cadence).
  auto _extract_gpu_particle_emitter(render_packet& packet, const assets::particle_emitter& config, scenes::particle_emitter& runtime, const scenes::particle_effect& instance, const math::matrix4x4& world, std::float_t delta_time) -> void;

  /**
   * @brief Advances @p animator (if not null) and samples @p skeleton's clip into @p pose's
   * joint_world_matrices/skinning_matrices. Called once per skinned instance from _build_packet,
   * at render cadence -- see scenes::skeleton_pose's doc comment for why this isn't a fixed_update.
   * A null @p animator (or one with an invalid/paused clip) evaluates the skeleton's bind pose.
   */
  auto _evaluate_skeleton_pose(const assets::skeleton& skeleton, scenes::animator* animator, scenes::skeleton_pose& pose, std::float_t delta_time) -> void;

  render_packet _work_packet{};
  bool _has_rendered{false};
  std::optional<camera_data> _camera_override{};

  std::uint32_t _sampler_index{0u};
  std::uint32_t _clamp_sampler_index{0u};
  bool _grid_enabled{false};

  graphics::image_handle _depth_image{};
  graphics::image_handle _color_image{};
  graphics::image_handle _color_msaa_image{};
  std::uint32_t _color_index{0u};

  graphics::image_handle _accum_image{};
  graphics::image_handle _accumulator_msaa_image{};
  std::uint32_t _accumulator_index{0u};
  graphics::image_handle _revealage_image{};
  graphics::image_handle _revealage_msaa_image{};
  std::uint32_t _revealage_index{0u};

  // Private mip chains bloom_pass reads/writes -- see bloom_pass.hpp. _bloom_upsample_index is the
  // one slot the rest of the module (tonemap_pass) needs, written against the image's own default
  // (whole-chain) view like _color_index is; tonemap.slang locks its bloom sample to mip 0 via
  // SampleLevel rather than needing a dedicated single-mip view. The internal per-mip views/indices
  // bloom_pass uses to build the chain are entirely private to bloom_pass itself.
  graphics::image_handle _bloom_downsample_image{};
  graphics::image_handle _bloom_upsample_image{};
  std::uint32_t _bloom_upsample_index{0u};

  math::vector2u _target_extent{};
  math::vector2u _viewport_extent{0u, 0u};

  render_graph _graph{};

  graphics::image_handle _final_image{};
  std::uint32_t _final_image_index{0u};

  graphics::buffer_handle _frame_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _frame_addresses{};

  graphics::buffer_handle _light_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _light_addresses{};

  graphics::buffer_handle _transform_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _transform_addresses{};

  // CPU-written every frame from packet.joint_matrices (skeleton_pose evaluation happens in
  // _build_packet, on the main thread) -- frame-in-flight multiplexed exactly like _transform_buffer,
  // for the same reason (a single-buffered host-visible buffer would race a still-in-flight
  // previous frame's GPU read).
  graphics::buffer_handle _joint_palette_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _joint_palette_addresses{};

  // GPU-written (skin_pass) and GPU-read (depth_pre_pass/shadow_pass/opaque_pass) only, entirely
  // within one frame's submission -- unlike the palette above, a single buffer (not per-frame-slot)
  // is safe here, protected by skin_pass's own cross-frame wait (see skin_pass.cpp).
  graphics::buffer_handle _skin_scratch_buffer{};
  graphics::buffer::address_type _skin_scratch_address{0u};

  graphics::buffer_handle _cluster_aabb_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _cluster_aabb_addresses{};

  graphics::buffer_handle _cluster_range_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _cluster_range_addresses{};

  graphics::buffer_handle _cluster_light_index_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _cluster_light_index_addresses{};

  graphics::buffer_handle _cluster_counter_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _cluster_counter_addresses{};

  std::array<graphics::image_handle, shadow_cascade_count> _shadow_map_images{};
  std::array<std::uint32_t, shadow_cascade_count> _shadow_map_indices{};

  render::debug_draw _debug_draw{};

  // GPU-path particles (see libsbx/render/particles/particle_pool.hpp) -- one pool per blend
  // mode, shared by every assets::particle_simulation_mode::gpu emitter in the scene.
  // unique_ptr rather than a by-value member: particle_pool is noncopyable and constructed after
  // _ensure_resources() so particle_simulate_pass can take stable references to both in the pass
  // list built by this constructor.
  std::unique_ptr<particle_pool> _particle_pool_additive{};
  std::unique_ptr<particle_pool> _particle_pool_alpha_blend{};

}; // class scene_renderer_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_SCENE_RENDERER_MODULE_HPP_
