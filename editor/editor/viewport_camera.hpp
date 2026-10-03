// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_CAMERA_HPP_
#define EDITOR_VIEWPORT_CAMERA_HPP_

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/scenes/components.hpp>

namespace editor {

struct viewport_camera_matrices {
  sbx::math::matrix4x4 view{sbx::math::matrix4x4::identity};
  sbx::math::matrix4x4 projection{sbx::math::matrix4x4::identity};
}; // struct viewport_camera_matrices

/** @brief The camera driving the viewport this frame, decided by editor_module::viewport_camera. */
struct viewport_camera_pose {
  sbx::math::matrix4x4 world_matrix{sbx::math::matrix4x4::identity};
  sbx::scenes::camera params{};
}; // struct viewport_camera_pose

/** @brief The view and projection for a camera at @p camera_world_matrix; shared by picking and the gizmo so they match what's rendered. */
[[nodiscard]] auto compute_viewport_camera_matrices(const sbx::math::matrix4x4& camera_world_matrix, const sbx::scenes::camera& camera, std::float_t aspect) -> viewport_camera_matrices;

} // namespace editor

#endif // EDITOR_VIEWPORT_CAMERA_HPP_
