// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_PACKET_HPP_
#define LIBSBX_RENDER_RENDER_PACKET_HPP_

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
  std::uint32_t padding{0u};
}; // struct light_data

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
