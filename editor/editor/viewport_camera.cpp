// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_camera.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>

namespace editor {

auto compute_viewport_camera_matrices(sbx::scenes::node& camera_node, const sbx::scenes::camera& camera, std::float_t aspect) -> viewport_camera_matrices {
  auto matrices = viewport_camera_matrices{};

  matrices.projection = sbx::math::matrix4x4::perspective(sbx::math::degree{camera.fov_degrees}, aspect, camera.near_plane, camera.far_plane);
  matrices.view = sbx::math::matrix4x4::inverted(camera_node.world_matrix());

  return matrices;
}

} // namespace editor
