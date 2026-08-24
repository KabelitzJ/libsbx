// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_RENDER_PACKET_HPP_
#define LIBSBX_RENDER_RENDER_PACKET_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/math/color.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/material.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/texture.hpp>
#include <libsbx/assets/environment_map.hpp>

#include <libsbx/render/particle_data.hpp>

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

struct camera_data {
  math::matrix4x4 view{math::matrix4x4::identity};
  math::vector3f position{0.0f, 0.0f, 0.0f};
  std::float_t fov_degrees{60.0f};
  std::float_t near_plane{0.1f};
  std::float_t far_plane{1000.0f};
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

struct particle_emitter_snapshot {
  std::uint32_t pool_index{0u};
  std::uint32_t gpu_slot{0u};
  emitter_instance data{};
}; // struct particle_emitter_snapshot

struct render_packet {
  math::color clear_color{0.05f, 0.05f, 0.08f, 1.0f};
  camera_data camera{};
  std::vector<draw_command> opaque_commands{};
  std::vector<draw_command> transparent_commands{};
  std::vector<math::matrix4x4> transforms{};
  std::vector<light_data> lights{};
  std::uint32_t directional_light_count{0u};
  assets::environment_map_handle environment{};
  std::float_t environment_intensity{1.0f};
  std::vector<particle_emitter_snapshot> particle_emitters{};
}; // struct render_packet

} // namespace sbx::render

#endif // LIBSBX_RENDER_RENDER_PACKET_HPP_
