// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SCENE_RENDERER_MODULE_HPP_
#define LIBSBX_RENDER_SCENE_RENDERER_MODULE_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

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
 * @brief Renders the active scene into an offscreen final_image every frame; knows nothing about the swapchain or ImGui.
 *
 * prepare() (main thread) extracts the scene into a render_packet; record(), possibly on the render thread, never touches the ECS.
 */
class scene_renderer_module final : public utility::noncopyable, public scene_renderer {

public:

  using dependencies = core::dependency_list<graphics::graphics_module, assets::assets_module, scenes::scenes_module, presentation_module>;

  scene_renderer_module();

  ~scene_renderer_module();

  auto prepare() -> void override;

  auto record(graphics::command_buffer& command_buffer, math::vector2u extent) -> void override;

  /**
   * @brief Overrides the render extent, e.g. with the editor's Viewport panel size.
   *
   * @param extent The extent, or {0, 0} for the swapchain extent.
   */
  auto set_viewport_extent(math::vector2u extent) -> void;

  /**
   * @brief The extent rendered at this frame: the override if set, otherwise the swapchain extent.
   *
   * @return The render extent.
   */
  [[nodiscard]] auto target_extent() const noexcept -> math::vector2u {
    return _target_extent;
  }

  auto set_viewport_offset(math::vector2 offset) -> void;

  [[nodiscard]] auto viewport_offset() const noexcept -> math::vector2 {
    return _viewport_offset;
  }

  /**
   * @brief Overrides the camera rendered with, e.g. the editor's fly camera. The environment and skybox still come from the scene's active camera.
   *
   * @param override The camera, or nullopt for the scene's active camera.
   */
  auto set_camera_override(std::optional<camera_data> override) -> void {
    _camera_override = override;
  }

  /**
   * @brief The camera this frame renders with: the override if set, otherwise the scene's active camera. World-space canvases follow it.
   *
   * @return The effective camera.
   */
  [[nodiscard]] auto effective_camera() -> camera_data;

  /**
   * @brief The final tonemapped image; the editor samples it for its Viewport panel.
   *
   * @return The image.
   */
  [[nodiscard]] auto final_image() const noexcept -> graphics::image_handle {
    return _final_image;
  }

  /**
   * @brief final_image's bindless sampled-image index.
   *
   * @return The index.
   */
  [[nodiscard]] auto final_image_index() const noexcept -> std::uint32_t {
    return _final_image_index;
  }

  /**
   * @brief The general-purpose material sampler's bindless index, also used for final_image.
   *
   * @return The index.
   */
  [[nodiscard]] auto sampler_index() const noexcept -> std::uint32_t {
    return _sampler_index;
  }

  /**
   * @brief Whether the last record() rendered anything; final_image is stale or unwritten otherwise.
   *
   * @return True if a camera was present.
   */
  [[nodiscard]] auto has_rendered() const noexcept -> bool {
    return _has_rendered;
  }

  /**
   * @brief Whether the last record() drew the shadow cascades, the only time shadow_map_preview_view() is safe to sample.
   *
   * @return True if shadows were rendered.
   */
  [[nodiscard]] auto has_rendered_shadows() const noexcept -> bool {
    return _has_rendered_shadows;
  }

  /**
   * @brief A grayscale-swizzled view of a cascade's depth map, for ImGui.
   *
   * @param cascade The cascade index.
   *
   * @return The image view.
   */
  [[nodiscard]] auto shadow_map_preview_view(std::uint32_t cascade) const noexcept -> VkImageView {
    return _shadow_map_preview_views[cascade];
  }

  /**
   * @brief Shows or hides the world-space reference grid. Off by default; the editor turns it on.
   *
   * @param enabled Whether to draw the grid.
   */
  auto set_grid_enabled(bool enabled) -> void;

  auto grid_enabled() const -> bool;

  /**
   * @brief Draws opaque geometry as wireframe, for runtime debugging. Off by default.
   *
   * @param enabled Whether to draw wireframe.
   */
  auto set_wireframe_enabled(bool enabled) -> void;

  auto wireframe_enabled() const -> bool;

  /**
   * @brief Tints lit surfaces by the shadow cascade they sample (red, green, blue, yellow; untinted beyond the shadow distance).
   *
   * @param enabled Whether to tint.
   */
  auto set_shadow_cascade_debug_enabled(bool enabled) -> void;

  auto shadow_cascade_debug_enabled() const -> bool;

  /**
   * @brief The shared immediate-mode line accumulator; debug_draw_pass draws and clears it every frame.
   *
   * @return The accumulator.
   */
  [[nodiscard]] auto debug_draw() noexcept -> render::debug_draw& {
    return _debug_draw;
  }

  /** @brief Discards every live GPU-path particle immediately, e.g. when play mode stops. */
  auto reset_particles() -> void;

  /** @brief Draw-call and instance counts per category from the last prepare(), for the Statistics panel. */
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

  /**
   * @brief Per-pass GPU timings, always max_frames_in_flight frames stale.
   *
   * @return The timings.
   */
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

  // Upper bound on draw commands per cull view per frame; overflow is skipped.
  inline static constexpr auto max_opaque_draw_commands = std::uint32_t{8192u};

  // Visible instanced instances per frame across all cull views; further renderers are skipped for the frame (logged once).
  inline static constexpr auto instanced_culled_capacity = std::uint32_t{262144u};

  // Totals across every skinned instance per frame; overflow isn't drawn.
  inline static constexpr auto joint_palette_capacity = std::uint32_t{4096u};
  inline static constexpr auto skin_scratch_vertex_capacity = std::uint32_t{65536u};

  auto _ensure_resources() -> void;

  auto _resize_targets(const math::vector2u extent) -> void;

  [[nodiscard]] auto _build_graph_resources() const -> graph_resources;

  auto _prepare_frame(render_context& context) -> void;

  [[nodiscard]] auto _build_packet() -> render_packet;

  /**
   * @brief The override camera if set, otherwise derived from the scene's active camera.
   *
   * @return The camera.
   */
  [[nodiscard]] auto _resolve_camera_data() -> camera_data;

  // GPU-path emitters: claims or keeps alive the emitter's pool slot and appends a snapshot if it is playing. Runs at render cadence.
  auto _extract_gpu_particle_emitter(render_packet& packet, const assets::particle_emitter& config, scenes::particle_emitter& runtime, const scenes::particle_effect& instance, const math::matrix4x4& world, std::float_t delta_time) -> void;

  /**
   * @brief Advances the animator's state machine and samples the (possibly crossfading) pose into @p pose. Runs once per skinned instance at render cadence.
   *
   * @param skeleton The skeleton being posed.
   * @param renderer The instance's mesh_renderer, for resolving clip names.
   * @param animator The animator, or null for the bind pose.
   * @param pose Receives joint world and skinning matrices.
   * @param delta_time The time step.
   */
  auto _evaluate_skeleton_pose(const assets::skeleton& skeleton, const scenes::mesh_renderer& renderer, scenes::animator* animator, scenes::skeleton_pose& pose, std::float_t delta_time) -> void;

  /**
   * @brief State machine bookkeeping only: current and target state, clip time and transition progress.
   *
   * @param renderer The instance's mesh_renderer, for resolving clip names.
   * @param graph The animation graph.
   * @param animator The animator to advance.
   * @param delta_time The time step.
   */
  auto _advance_animator_state(const scenes::mesh_renderer& renderer, const assets::animation_graph& graph, scenes::animator& animator, std::float_t delta_time) -> void;

  /**
   * @brief The clip named by @p state, resolved against the renderer's mesh.
   *
   * @param renderer The mesh_renderer whose mesh provides the clips.
   * @param state The animation state.
   *
   * @return The clip, or an invalid handle if @p state is null or the clip or mesh is missing.
   */
  [[nodiscard]] auto _resolve_state_clip(const scenes::mesh_renderer& renderer, const assets::animation_state* state) const -> assets::animation_clip_handle;

  struct draw_bucket {
    assets::mesh_handle mesh{};
    std::uint32_t submesh_index{0u};
    assets::material_handle material{};
    std::uint32_t pipeline_id{0u};
    std::vector<transform_data> transforms{};
  }; // struct draw_bucket

  struct transparent_entry {
    assets::mesh_handle mesh{};
    std::uint32_t submesh_index{0u};
    assets::material_handle material{};
    std::uint32_t pipeline_id{0u};
    transform_data transform{};
  }; // struct transparent_entry

  render_packet _work_packet{};
  bool _has_rendered{false};
  std::optional<camera_data> _camera_override{};

  // Scratch buffers reused across instances and frames; _build_packet's skinned loop is serial.
  std::vector<math::vector3> _skeleton_scratch_translations{};
  std::vector<math::quaternion> _skeleton_scratch_rotations{};
  std::vector<math::vector3> _skeleton_scratch_scales{};
  std::vector<math::vector3> _skeleton_scratch_target_translations{};
  std::vector<math::quaternion> _skeleton_scratch_target_rotations{};
  std::vector<math::vector3> _skeleton_scratch_target_scales{};
  std::vector<math::matrix4x4> _skeleton_scratch_locals{};

  // Reused across frames to avoid reallocating; buckets left empty by the previous build are pruned.
  std::unordered_map<mesh_key, draw_bucket, mesh_key_hash> _opaque_buckets{};
  std::vector<std::pair<mesh_key, const draw_bucket*>> _ordered_opaque_buckets{};
  std::vector<transparent_entry> _transparent_entries{};

  draw_stats _last_draw_stats{};

  std::uint32_t _sampler_index{0u};
  std::uint32_t _clamp_sampler_index{0u};
  std::uint32_t _shadow_sampler_index{0u};
  bool _grid_enabled{false};
  bool _wireframe_enabled{false};
  bool _shadow_cascade_debug_enabled{false};
  bool _has_rendered_shadows{false};
  std::array<VkImageView, shadow_cascade_count> _shadow_map_preview_views{};

  graphics::image_handle _depth_image{};
  // Single-sample resolve of _depth_image, sampled by the shader graph Scene Depth node.
  graphics::image_handle _scene_depth_image{};
  std::uint32_t _scene_depth_index{0u};
  // Half-resolution ambient occlusion targets: raw, then blurred (the one lighting reads).
  graphics::image_handle _ambient_occlusion_raw_image{};
  graphics::image_handle _ambient_occlusion_image{};
  std::uint32_t _ambient_occlusion_raw_index{0u};
  std::uint32_t _ambient_occlusion_index{0u};
  std::uint32_t _ambient_occlusion_raw_storage_index{0xFFFFFFFFu};
  std::uint32_t _ambient_occlusion_storage_index{0xFFFFFFFFu};
  graphics::image_handle _color_image{};
  graphics::image_handle _color_msaa_image{};
  std::uint32_t _color_index{0u};

  graphics::image_handle _accum_image{};
  graphics::image_handle _accumulator_msaa_image{};
  std::uint32_t _accumulator_index{0u};
  graphics::image_handle _revealage_image{};
  graphics::image_handle _revealage_msaa_image{};
  std::uint32_t _revealage_index{0u};

  // bloom_pass's private mip chains; tonemap_pass only needs _bloom_upsample_index.
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

  // frustum_cull_pass output, consumed by depth_pre_pass and opaque_pass instead of _transform_buffer.
  graphics::buffer_handle _culled_indirect_args_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _culled_indirect_args_addresses{};

  graphics::buffer_handle _culled_transform_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _culled_transform_addresses{};
  graphics::buffer_handle _instanced_culled_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _instanced_culled_addresses{};

  // Written by the CPU every frame; one region per frame in flight so the previous frame's GPU read doesn't race.
  graphics::buffer_handle _joint_palette_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _joint_palette_addresses{};

  // Written by skin_pass and read by the geometry passes; one region per frame slot, as the previous frame may still read its region.
  graphics::buffer_handle _skin_scratch_buffer{};
  std::array<graphics::buffer::address_type, graphics::swapchain::max_frames_in_flight> _skin_scratch_addresses{};

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

  // GPU-path particle pools, one per blend mode. unique_ptr because they are built after _ensure_resources() and passes keep references to them.
  std::unique_ptr<particle_pool> _particle_pool_additive{};
  std::unique_ptr<particle_pool> _particle_pool_alpha_blend{};

}; // class scene_renderer_module

} // namespace sbx::render

#endif // LIBSBX_RENDER_SCENE_RENDERER_MODULE_HPP_
