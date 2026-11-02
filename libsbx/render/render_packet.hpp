// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_PACKET_HPP_
#define LIBSBX_RENDER_RENDER_PACKET_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/material.hpp>
#include <libsbx/assets/mesh.hpp>

#include <libsbx/scenes/post_process.hpp>
#include <libsbx/scenes/instance_buffer.hpp>
#include <libsbx/assets/texture2d.hpp>
#include <libsbx/assets/environment_map.hpp>
#include <libsbx/assets/particle_effect.hpp>

#include <libsbx/utility/hash.hpp>

#include <libsbx/render/particles/particle_data.hpp>

namespace sbx::render {

/**
 * @brief Identity of a coalesced draw; draws sharing a key collapse into one instanced draw.
 *
 * Ordered mesh -> submesh -> material so a mesh's submeshes stay adjacent (index buffer binds once).
 */
struct mesh_key {

  math::uuid mesh{math::uuid::nil()};
  std::uint32_t submesh{0u};
  math::uuid material{math::uuid::nil()};

  auto operator==(const mesh_key& other) const -> bool {
    return mesh == other.mesh && submesh == other.submesh && material == other.material;
  }

  auto operator<(const mesh_key& other) const -> bool {
    if (mesh < other.mesh) { 
      return true; 
    }

    if (other.mesh < mesh) { 
      return false; 
    }

    if (submesh < other.submesh) { 
      return true; 
    }

    if (other.submesh < submesh) { 
      return false; 
    }

    return material < other.material;
  }

}; // struct mesh_key

/** @brief Hashes a mesh_key; buckets are accumulated unordered, then sorted once by mesh_key. */
struct mesh_key_hash {
  auto operator()(const mesh_key& key) const noexcept -> std::size_t {
    auto seed = std::hash<math::uuid>{}(key.mesh);
    utility::hash_combine(seed, key.submesh, std::hash<math::uuid>{}(key.material));
    return seed;
  }
}; // struct mesh_key_hash

struct draw_command {
  assets::mesh_handle mesh{};
  std::uint32_t submesh_index{0u};
  assets::material_handle material{};
  std::uint32_t instance_count{0u};
  std::uint32_t transform_offset{0u};
  std::uint32_t pipeline_id{0u};

  // 0 reads the mesh's static vertex_address(); otherwise the skinned instance's scratch buffer.
  graphics::buffer::address_type vertex_address_override{0u};

  // Local bounds tested by frustum_cull_pass; inflated only for skinned instances.
  math::volume local_bounds{};

  // Non-null for instanced draws: instances come from this buffer, and transform_offset is the renderer's node transform. Visible instances land in the culled pool at culled_offset.
  std::shared_ptr<const scenes::instance_buffer> instances{};
  std::uint32_t culled_offset{0u};

  // Detail levels frustum_cull picks from per instance: level 0 is the submesh, level k its k-th cooked simplification, whose meshopt error is lod_errors[k - 1].
  std::uint32_t lod_count{1u};
  std::array<std::float_t, 4u> lod_errors{};

  // Where this command's culled output sits within a cull view, per command list (opaque and shadow caster lists lay out separately):
  // its lod_count indirect args start at lod_args_offset, and level k's visible transforms (non-instanced only) at lod_culled_offset + k * instance_count.
  std::uint32_t lod_args_offset{0u};
  std::uint32_t lod_culled_offset{0u};

  // Camera-view occlusion: this command's first index in the per-instance visibility buffer (opaque list only).
  std::uint32_t occlusion_offset{0u};

  // Instanced only: how many view blocks of lod_count * instance_count transforms it reserved; the last is the late camera view.
  std::uint32_t instanced_view_blocks{0u};

  // Resolved once, since several passes submit the same command list and residency can't change mid-frame.
  bool resident{false};
}; // struct draw_command

/**
 * @brief Per-instance world matrix, inverse-transpose normal matrix, color and custom_data. Mirrors frame_data.slang's transform_data.
 *
 * The normal matrix is computed once (on the CPU, or on the GPU for instanced draws) so normals survive non-uniform scale.
 */
struct transform_data {
  math::matrix4x4 model{math::matrix4x4::identity};
  math::matrix4x4 normal{math::matrix4x4::identity};
  math::color color{math::color::white()};
  math::vector4 custom_data{0.0f, 0.0f, 0.0f, 0.0f};
}; // struct transform_data

/**
 * @brief One skinned instance's skin_pass dispatch, skinning its whole vertex range once for every submesh draw.
 *
 * joint_offset indexes this frame's joint palette; output_vertex_address is a final address copied into the matching draw_command::vertex_address_override.
 */
struct skin_dispatch {
  graphics::buffer::address_type source_vertex_address{0u};
  graphics::buffer::address_type source_skin_vertex_address{0u};
  graphics::buffer::address_type output_vertex_address{0u};
  std::uint32_t joint_offset{0u};
  std::uint32_t vertex_count{0u};
}; // struct skin_dispatch

struct camera_data {
  math::matrix4x4 view{math::matrix4x4::identity};
  math::vector3 position{0.0f, 0.0f, 0.0f};
  std::float_t fov_degrees{60.0f};
  std::float_t near_plane{0.1f};
  std::float_t far_plane{1000.0f};
  scenes::post_process_settings post_process{};
  bool is_active{false};
}; // struct camera_data

enum class light_type : std::uint32_t {
  directional = 0u,
  point = 1u,
  spot = 2u
}; // enum class light_type

struct light_data {
  math::vector4 color{1.0f, 1.0f, 1.0f, 1.0f}; // rgb + intensity in a
  math::vector4 position{0.0f, 0.0f, 0.0f, 0.0f}; // xyz + range in w
  math::vector4 direction{0.0f, 0.0f, -1.0f, 0.0f};
  light_type type{light_type::directional};
  std::float_t inner_cos{0.0f};
  std::float_t outer_cos{0.0f};
  std::uint32_t shadow_index{0xFFFFFFFFu}; // index into render_packet::local_shadow_views, 0xFFFFFFFF if the light casts no shadow this frame
}; // struct light_data

/**
 * @brief One local light shadow tile, shadow view shadow_cascade_count + its index. Mirrors frame_data.slang's local_shadow_view.
 */
struct local_shadow_view {
  math::matrix4x4 view_projection{math::matrix4x4::identity};
  math::vector4 atlas_rect{0.0f, 0.0f, 0.0f, 0.0f}; // xy: top-left, zw: size, both in atlas uv
  math::vector4 bias{0.0f, 0.0f, 0.0f, 0.0f}; // x: depth bias, y: normal bias (both in texels), z: one texel's world size at unit distance, w: unused
}; // struct local_shadow_view

/**
 * @brief One projected decal, mirrors frame_data.slang's decal_data.
 *
 * The decal is the unit cube [-0.5, 0.5]^3 under its node's world transform, projecting along its -Z.
 */
struct decal_data {
  math::matrix4x4 world_to_decal{math::matrix4x4::identity};
  math::vector4 center{0.0f, 0.0f, 0.0f, 0.0f}; // xyz: world-space box center
  math::vector4 axis_x{0.5f, 0.0f, 0.0f, 1.0f}; // xyz: world-space half extent along X, w: normal strength
  math::vector4 axis_y{0.0f, 0.5f, 0.0f, 0.5f}; // xyz: along Y, w: cosine where the angle fade starts
  math::vector4 axis_z{0.0f, 0.0f, 0.5f, 0.0f}; // xyz: along Z (back toward the projector), w: cosine where the decal is gone
  math::vector4 color{1.0f, 1.0f, 1.0f, 1.0f}; // rgb: tint, a: opacity
  std::uint32_t albedo_index{0xFFFFFFFFu};
  std::uint32_t normal_index{0xFFFFFFFFu};
  std::float_t edge_fade{0.1f};
  std::uint32_t padding{0u};
}; // struct decal_data

/**
 * @brief One baked reflection probe, mirrors frame_data.slang's probe_data.
 *
 * The box is centered on the capture point and turns with the probe's node, ignoring its scale.
 */
struct probe_data {
  math::matrix4x4 world_to_box{math::matrix4x4::identity};
  math::vector4 center{0.0f, 0.0f, 0.0f, 0.0f}; // xyz: world-space box center, w: blend distance
  math::vector4 axis_x{1.0f, 0.0f, 0.0f, 1.0f}; // xyz: world-space half extent along X, w: intensity
  math::vector4 axis_y{0.0f, 1.0f, 0.0f, 1.0f}; // xyz: along Y, w: 1 for box projection
  math::vector4 axis_z{0.0f, 0.0f, 1.0f, 0.0f}; // xyz: along Z
  math::vector4 extents{1.0f, 1.0f, 1.0f, 0.0f}; // xyz: half extents in box space
  std::uint32_t irradiance_index{0u};
  std::uint32_t prefiltered_index{0u};
  std::uint32_t prefiltered_mip_count{0u};
  std::uint32_t padding{0u};
}; // struct probe_data

/** @brief One camera-facing quad, vertex-pulled by particle_pass. */
struct particle_billboard_instance {
  math::vector3 position{0.0f, 0.0f, 0.0f};
  std::float_t size{0.0f};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::float_t rotation{0.0f};
  std::uint32_t texture_index{0xFFFFFFFFu};
  math::vector2 padding{0.0f, 0.0f};
}; // struct particle_billboard_instance

/** @brief One instanced draw of billboards sharing a texture and blend mode. */
struct particle_billboard_command {
  assets::emitter_blend_mode blend_mode{assets::emitter_blend_mode::additive};
  std::uint32_t instance_count{0u};
  std::uint32_t instance_offset{0u};
}; // struct particle_billboard_command

/** @brief One particle drawn as an unlit mesh instance, carrying a color instead of a normal matrix. */
struct particle_mesh_instance {
  math::matrix4x4 model{math::matrix4x4::identity};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
}; // struct particle_mesh_instance

/** @brief One instanced draw of mesh particles sharing a mesh, submesh, material and blend mode. */
struct particle_mesh_command {
  assets::emitter_blend_mode blend_mode{assets::emitter_blend_mode::additive};
  assets::mesh_handle mesh{};
  std::uint32_t submesh_index{0u};
  assets::material_handle material{};
  std::uint32_t instance_count{0u};
  std::uint32_t instance_offset{0u};
}; // struct particle_mesh_command

/** @brief One vertex of a trail ribbon, with width and camera facing baked in at extraction. */
struct trail_vertex {
  math::vector3 position{0.0f, 0.0f, 0.0f};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
}; // struct trail_vertex

/** @brief One triangle-list draw of trail vertices sharing a blend mode. */
struct particle_trail_command {
  assets::emitter_blend_mode blend_mode{assets::emitter_blend_mode::additive};
  std::uint32_t vertex_count{0u};
  std::uint32_t vertex_offset{0u};
}; // struct particle_trail_command

/** @brief One GPU-path emitter's per-frame data for particle_simulate_pass; pool_index selects the pool owning `slot`. */
struct particle_emitter_snapshot {
  std::uint32_t pool_index{0u};
  std::uint32_t slot{0u};
  emitter_instance data{};
}; // struct particle_emitter_snapshot

struct render_packet {
  camera_data camera{};
  std::vector<draw_command> opaque_commands{};
  std::vector<draw_command> transparent_commands{};
  std::vector<draw_command> shadow_caster_commands{};
  std::vector<draw_command> selected_commands{}; // editor selection outline: one single-instance draw per selected submesh, opaque or not
  std::vector<transform_data> transforms{};
  std::vector<light_data> lights{};
  std::uint32_t directional_light_count{0u};
  std::vector<particle_billboard_instance> particle_billboard_instances{};
  std::vector<particle_billboard_command> particle_billboard_commands{};
  std::vector<particle_mesh_instance> particle_mesh_instances{};
  std::vector<particle_mesh_command> particle_mesh_commands{};
  std::vector<trail_vertex> trail_vertices{};
  std::vector<particle_trail_command> trail_commands{};
  std::vector<particle_emitter_snapshot> particle_emitters{}; // GPU-path emitters only
  std::vector<math::matrix4x4> joint_matrices{}; // every skinned instance's skinning matrices, concatenated
  std::vector<skin_dispatch> skin_dispatches{};
  bool has_shadow_caster{false}; // lights[0] is the cascaded-shadow-mapped sun

  // Local light shadow tiles: shadow view shadow_cascade_count + i uses local_shadow_views[i] and culls shadow_caster_commands.
  std::vector<local_shadow_view> local_shadow_views{};

  // Every active decal whose textures are resident, in sort order.
  std::vector<decal_data> decals{};

  // Every active reflection probe with a resident bake, most important first (higher importance, then smaller box).
  std::vector<probe_data> probes{};

  // Per local shadow view: 1 if its tile must be redrawn this frame, 0 if the atlas still holds it from last frame.
  std::vector<std::uint8_t> local_shadow_dirty{};

  // Culled-pool slots the instanced commands reserved, summed over their cull views and LOD levels (see draw_command::culled_offset).
  std::uint32_t instanced_culled_count{0u};

  // Per cull view: indirect args and non-instanced culled transforms its command list needs (the larger of the opaque and shadow caster lists').
  std::uint32_t cull_args_stride{0u};
  std::uint32_t cull_transform_stride{0u};

  // Camera-view instances with an occlusion visibility entry: every transform, then each instanced command's instances.
  std::uint32_t occlusion_instance_count{0u};
  std::float_t shadow_distance{75.0f};
  std::float_t shadow_depth_bias{1.0f};
  std::float_t shadow_normal_bias{1.0f};
  std::float_t shadow_angular_diameter{0.53f}; // degrees
  std::float_t contact_shadow_length{0.0f};
  assets::environment_map_handle environment{};
  std::float_t environment_intensity{1.0f};
  std::float_t ambient_intensity{1.0f};
  std::float_t time{0.0f};
  std::float_t delta_time{0.0f};
}; // struct render_packet

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_PACKET_HPP_
