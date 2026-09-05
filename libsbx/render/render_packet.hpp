// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_PACKET_HPP_
#define LIBSBX_RENDER_RENDER_PACKET_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/material.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/environment_map.hpp>
#include <libsbx/assets/particle_effect.hpp>

namespace sbx::render {

/**
 * @brief Identity of a coalesced draw. Draws sharing a key (same mesh, submesh and material) differ only by transform and collapse into one instanced draw. 
 * Ordered mesh -> submesh -> material so a mesh's submeshes stay adjacent (its index buffer binds once).
 */
struct mesh_key {

  math::uuid mesh{math::uuid::nil()};
  std::uint32_t submesh{0u};
  math::uuid material{math::uuid::nil()};

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

struct draw_command {
  assets::mesh_handle mesh{};
  std::uint32_t submesh_index{0u};
  assets::material_handle material{};
  std::uint32_t instance_count{0u};
  std::uint32_t transform_offset{0u};
  std::uint32_t pipeline_id{0u};
}; // struct draw_command

/**
 * @brief Per-instance world matrix paired with its inverse-transpose normal matrix, computed once
 * on the CPU (libsbx::math::matrix4x4::inverted/transposed) rather than re-derived on the GPU, so
 * normals stay correct under non-uniform scale/skew. The two float4x4s pack with no padding.
 */
struct transform_data {
  math::matrix4x4 model{math::matrix4x4::identity};
  math::matrix4x4 normal{math::matrix4x4::identity};
}; // struct transform_data

struct camera_data {
  math::matrix4x4 view{math::matrix4x4::identity};
  math::vector3 position{0.0f, 0.0f, 0.0f};
  std::float_t fov_degrees{60.0f};
  std::float_t near_plane{0.1f};
  std::float_t far_plane{1000.0f};
  std::float_t exposure{0.0f};
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

/**
 * @brief One camera-facing quad, vertex-pulled by particle_pass (see shaders/particles/particle_billboard.slang).
 */
struct particle_billboard_instance {
  math::vector3 position{0.0f, 0.0f, 0.0f};
  std::float_t size{0.0f};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::float_t rotation{0.0f};
  std::uint32_t texture_index{0xFFFFFFFFu};
  math::vector2 padding{0.0f, 0.0f};
}; // struct particle_billboard_instance

/**
 * @brief One coalesced instanced draw of particle_billboard_instances sharing a texture and blend mode.
 */
struct particle_billboard_command {
  assets::emitter_blend_mode blend_mode{assets::emitter_blend_mode::additive};
  std::uint32_t instance_count{0u};
  std::uint32_t instance_offset{0u};
}; // struct particle_billboard_command

/**
 * @brief One particle rendered as an instanced mesh instead of a billboard -- unlit (texture * color,
 * no lighting), matching the billboard particle's own unlit style, vertex-pulled from the mesh's own
 * buffer like an ordinary transform_data instance but with a per-particle color instead of a normal
 * matrix (there's no lighting to correct normals for).
 */
struct particle_mesh_instance {
  math::matrix4x4 model{math::matrix4x4::identity};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
}; // struct particle_mesh_instance

/**
 * @brief One coalesced instanced draw of particle_mesh_instances sharing a mesh, submesh, material and blend mode.
 */
struct particle_mesh_command {
  assets::emitter_blend_mode blend_mode{assets::emitter_blend_mode::additive};
  assets::mesh_handle mesh{};
  std::uint32_t submesh_index{0u};
  assets::material_handle material{};
  std::uint32_t instance_count{0u};
  std::uint32_t instance_offset{0u};
}; // struct particle_mesh_command

/**
 * @brief One vertex of a CPU-expanded trail ribbon (see shaders/particles/trail.slang) -- the width
 * extrusion and camera-facing orientation are already baked in at extraction time
 * (scene_renderer_module.cpp), so the shader itself only needs to transform position to clip space.
 */
struct trail_vertex {
  math::vector3 position{0.0f, 0.0f, 0.0f};
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
}; // struct trail_vertex

/**
 * @brief One coalesced non-indexed triangle-list draw of trail_vertices sharing a blend mode --
 * trails render with the same blend mode as their owning emitter.
 */
struct particle_trail_command {
  assets::emitter_blend_mode blend_mode{assets::emitter_blend_mode::additive};
  std::uint32_t vertex_count{0u};
  std::uint32_t vertex_offset{0u};
}; // struct particle_trail_command

struct render_packet {
  camera_data camera{};
  std::vector<draw_command> opaque_commands{};
  std::vector<draw_command> transparent_commands{};
  std::vector<draw_command> shadow_caster_commands{};
  std::vector<transform_data> transforms{};
  std::vector<light_data> lights{};
  std::uint32_t directional_light_count{0u};
  std::vector<particle_billboard_instance> particle_billboard_instances{};
  std::vector<particle_billboard_command> particle_billboard_commands{};
  std::vector<particle_mesh_instance> particle_mesh_instances{};
  std::vector<particle_mesh_command> particle_mesh_commands{};
  std::vector<trail_vertex> trail_vertices{};
  std::vector<particle_trail_command> trail_commands{};
  bool has_shadow_caster{false}; // When true, lights[0] is the cascaded-shadow-mapped sun.
  std::float_t shadow_distance{75.0f};
  assets::environment_map_handle environment{};
  std::float_t environment_intensity{1.0f};
  std::float_t ambient_intensity{1.0f};
  std::float_t time{0.0f};
  std::float_t delta_time{0.0f};
}; // struct render_packet

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_PACKET_HPP_
