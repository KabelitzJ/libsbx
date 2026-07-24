// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_COMPONENTS_HPP_
#define LIBSBX_SCENES_COMPONENTS_HPP_

#include <optional>
#include <vector>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/ecs/entity.hpp>

#include <libsbx/assets/material.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::scenes {

/**
 * @brief The authored transform of a node, relative to its parent.
 * Its derived from the world transform each frame by scene::update() and written to the world_transform component.
 */
struct local_transform {

  math::vector3f position{0.0f, 0.0f, 0.0f};
  math::quaternion rotation{math::quaternion::identity};
  math::vector3f scale{1.0f, 1.0f, 1.0f};

  [[nodiscard]] auto matrix() const -> math::matrix4x4 {
    const auto translation_matrix = math::matrix4x4::translated(math::matrix4x4::identity, position);
    const auto scale_matrix = math::matrix4x4::scaled(math::matrix4x4::identity, scale);

    return translation_matrix * math::matrix_cast<math::matrix4x4>(rotation) * scale_matrix;
  }
}; // struct local_transform

/**
 * @brief Cached world matrix, written each frame by scene::update().
 */
struct world_transform {
  math::matrix4x4 matrix{math::matrix4x4::identity};
}; // struct world_transform

struct relationship {
  ecs::entity parent{ecs::null_entity};
  std::vector<ecs::entity> children{};
}; // struct relationship

struct camera {
  float fov_degrees{60.0f};
  float near_plane{0.1f};
  float far_plane{1000.0f};
}; // struct camera

/**
 * @brief What a node draws. Holds runtime handles for now; becomes asset uuids in F3.
 */
struct mesh_renderer {
  assets::mesh_handle mesh{};
  assets::material_handle material{};
}; // struct mesh_renderer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_HPP_
