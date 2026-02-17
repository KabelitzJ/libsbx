// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PHYSICS_MESH_COLLIDER_HPP_
#define LIBSBX_PHYSICS_MESH_COLLIDER_HPP_

#include <vector>
#include <optional>
#include <cstdint>
#include <filesystem>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/quaternion.hpp>

#include <libsbx/physics/shape_collider.hpp>

namespace sbx::physics {

class mesh_collider {

public:

  struct bvh_node {
    math::volume volume;
    std::uint32_t first; // child index if internal, triangle_indices offset if leaf
    std::uint32_t count; // 0 = internal, >0 = leaf triangle count
  }; // struct bvh_node

  mesh_collider(const std::filesystem::path& path);

  mesh_collider(std::vector<math::vector3> vertices, std::vector<std::uint32_t> indices);

  auto triangle_count() const noexcept -> std::uint32_t;

  auto get_triangle(std::uint32_t triangle_index) const -> triangle;

  auto query_bvh(const math::volume& volume) const -> std::vector<std::uint32_t>;

private:

  auto _build_bvh() -> void;

  auto _build_bvh_recursive(std::uint32_t node_index, std::uint32_t triangle_begin, std::uint32_t triangle_count) -> void;

  auto _triangle_centroid(std::uint32_t triangle_index) const -> math::vector3;

  auto _triangle_volume(std::uint32_t triangle_index) const -> math::volume;

  std::vector<math::vector3> _vertices;
  std::vector<std::uint32_t> _indices;

  std::vector<bvh_node> _nodes;
  std::vector<std::uint32_t> _triangle_indices;

}; // class mesh_collider

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_MESH_COLLIDER_HPP_