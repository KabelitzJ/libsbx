// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SCENE_RENDERER_MODULE_HPP_
#define LIBSBX_RENDER_SCENE_RENDERER_MODULE_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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
   * @brief The extent actually being rendered at this frame -- set_viewport_extent()'s override
   * when one is active, otherwise the swapchain's own extent. What Sbx.Core.Camera.Viewport/
   * ScreenPointToRay derive their aspect ratio from (see interop::camera_get_viewport).
   */
  [[nodiscard]] auto target_extent() const noexcept -> math::vector2u {
    return _target_extent;
  }

  auto set_viewport_offset(math::vector2 offset) -> void;

  [[nodiscard]] auto viewport_offset() const noexcept -> math::vector2 {
    return _viewport_offset;
  }

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
   * @brief The camera_data this frame is actually (or would actually) be rendered with -- the
   * override if one is set, otherwise derived fresh from the scene's active camera, exactly the
   * same resolution _build_packet() itself uses. canvas_module calls this for world_space canvases
   * specifically so they track whatever camera the viewport is really looking through (the editor's
   * own fly-camera while play_state is "edit", the scene's own camera otherwise) instead of always
   * assuming scene.active_camera() is the one actually rendering -- see canvas_module::update()'s
   * doc comment on why screen_space_camera canvases deliberately don't use this (they're pinned to
   * an explicit camera reference instead, the same way Unity's Canvas.worldCamera field is).
   */
  [[nodiscard]] auto effective_camera() -> camera_data;

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
   * @brief Draws opaque geometry as wireframe (see opaque_pass) instead of filled triangles. Off
   * by default. Unlike grid_enabled, this is meant to be flipped at runtime for debugging (e.g. a
   * script bound to a hotkey), not just by the editor.
   */
  auto set_wireframe_enabled(bool enabled) -> void;

  auto wireframe_enabled() const -> bool;

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

  /** @brief Draw-call/instance counts per category, from the most recent prepare() call -- for the editor's Statistics panel. */
  struct draw_category_stats {
    std::uint32_t draw_calls{0u};
    std::uint32_t instance_count{0u};
  }; // struct draw_category_stats

  struct draw_stats {
    draw_category_stats opaque{};
    draw_category_stats transparent{};
    draw_category_stats shadow{};
  }; // struct draw_stats

  [[nodiscard]] auto last_draw_stats() const noexcept -> const draw_stats& {
    return _last_draw_stats;
  }

  /** @ref render_graph::pass_timings -- always max_frames_in_flight frames stale. */
  [[nodiscard]] auto pass_timings() const noexcept -> std::span<const pass_gpu_timing> {
    return _graph.pass_timings();
  }

  [[nodiscard]] auto pipeline_stats() const noexcept -> const render::pipeline_statistics& {
    return _graph.pipeline_stats();
  }

  [[nodiscard]] auto total_gpu_time_ms() const noexcept -> std::float_t {
    return _graph.total_gpu_time_ms();
  }

private:

  inline static constexpr auto light_capacity = std::uint32_t{256u};
  inline static constexpr auto transform_capacity = std::uint32_t{16384u};
  inline static constexpr auto cluster_light_index_capacity = std::uint32_t{65536u};

  // frustum_cull_pass: a fixed upper bound on opaque draw commands per frame (mirroring
  // transform_capacity's "just skip the overflow" v1 policy above) and the compacted,
  // visibility-culled transform buffer it writes into -- same capacity as _transform_buffer, since
  // worst case every instance survives culling and needs its full original slot.
  inline static constexpr auto max_opaque_draw_commands = std::uint32_t{8192u};

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

  /** @brief Shared by _build_packet() and the public effective_camera() -- the override if set, else derived from scene.active_camera(). Bloom/exposure come along with it; environment/skybox stay separately scene-authored (see _build_packet's own comment on that). */
  [[nodiscard]] auto _resolve_camera_data() -> camera_data;

  // GPU-path only (assets::particle_simulation_mode::gpu) -- claims/keeps-alive this
  // emitter's particle_pool slot and appends a particle_emitter_snapshot to packet.particle_emitters
  // if it's currently playing. See scenes::particle_emitter::slot's doc comment for why this
  // bookkeeping lives here (render cadence) rather than particles_module (fixed-step cadence).
  auto _extract_gpu_particle_emitter(render_packet& packet, const assets::particle_emitter& config, scenes::particle_emitter& runtime, const scenes::particle_effect& instance, const math::matrix4x4& world, std::float_t delta_time) -> void;

  /**
   * @brief Advances @p animator's graph state machine (if not null) and samples the resulting
   * pose -- crossfading between the outgoing/incoming clip when mid-transition -- into @p pose's
   * joint_world_matrices/skinning_matrices. Called once per skinned instance from _build_packet,
   * at render cadence -- see scenes::skeleton_pose's doc comment for why this isn't a fixed_update.
   * A null @p animator (or one with no graph assigned) evaluates the skeleton's bind pose.
   * @p renderer is the same instance's mesh_renderer -- needed to resolve an
   * assets::animation_state::clip_name against its mesh's clip list.
   */
  auto _evaluate_skeleton_pose(const assets::skeleton& skeleton, const scenes::mesh_renderer& renderer, scenes::animator* animator, scenes::skeleton_pose& pose, std::float_t delta_time) -> void;

  /** @brief Pure state-machine bookkeeping (current/transition-target state, local clip time, transition progress) -- no joint/vertex math. Resolves current_clip/transition_target_clip against @p renderer's mesh by name as states are (re-)entered. */
  auto _advance_animator_state(const scenes::mesh_renderer& renderer, const assets::animation_graph& graph, scenes::animator& animator, std::float_t delta_time) -> void;

  /** @brief The clip named by @p state's clip_name, resolved against @p renderer's mesh->animation_clips(); an invalid handle if @p state is null, the mesh has no such clip, or the mesh itself isn't assigned. */
  [[nodiscard]] auto _resolve_state_clip(const scenes::mesh_renderer& renderer, const assets::animation_state* state) const -> assets::animation_clip_handle;

  render_packet _work_packet{};
  bool _has_rendered{false};
  std::optional<camera_data> _camera_override{};

  // Scratch buffers for _evaluate_skeleton_pose, reused across every skinned instance/frame instead
  // of freshly heap-allocated per call -- safe because _build_packet's skinned-mesh loop is strictly
  // serial (no threading/job-system involved), so no two calls to _evaluate_skeleton_pose are ever
  // in flight at once.
  std::vector<math::vector3> _skeleton_scratch_translations{};
  std::vector<math::quaternion> _skeleton_scratch_rotations{};
  std::vector<math::vector3> _skeleton_scratch_scales{};
  std::vector<math::vector3> _skeleton_scratch_target_translations{};
  std::vector<math::quaternion> _skeleton_scratch_target_rotations{};
  std::vector<math::vector3> _skeleton_scratch_target_scales{};
  std::vector<math::matrix4x4> _skeleton_scratch_locals{};

  // Unique opaque mesh/submesh/material bucket count from the last _build_packet call, used to
  // reserve() the accumulation map up front instead of growing it one rehash at a time.
  std::size_t _last_opaque_bucket_count{128u};

  draw_stats _last_draw_stats{};

  std::uint32_t _sampler_index{0u};
  std::uint32_t _clamp_sampler_index{0u};
  bool _grid_enabled{false};
  bool _wireframe_enabled{false};

  graphics::image_handle _depth_image{};
  // _depth_image's own MSAA resolve target (depth_pre_pass.hpp's own doc comment) -- single-sample,
  // bindless-sampleable, read by the shader graph Scene Depth node via _scene_depth_index below.
  graphics::image_handle _scene_depth_image{};
  std::uint32_t _scene_depth_index{0u};
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
  math::vector2 _viewport_offset{0.0f, 0.0f};

  render_graph _graph{};

  graphics::image_handle _final_image{};
  std::uint32_t _final_image_index{0u};

  graphics::buffer_handle _frame_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _frame_addresses{};

  graphics::buffer_handle _light_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _light_addresses{};

  graphics::buffer_handle _transform_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _transform_addresses{};

  // frustum_cull_pass's output for this frame's opaque_commands -- see render_context's own doc
  // comment on culled_indirect_args_buffer/culled_transform_address for how depth_pre_pass/
  // opaque_pass consume these instead of _transform_buffer.
  graphics::buffer_handle _culled_indirect_args_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _culled_indirect_args_addresses{};

  graphics::buffer_handle _culled_transform_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _culled_transform_addresses{};

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
