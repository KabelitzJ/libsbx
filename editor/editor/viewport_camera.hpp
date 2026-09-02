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

/**
 * @brief Whichever camera is actually driving the viewport this frame — the editor camera while
 * editing, or the scene's own play camera while playing/paused. See
 * editor_module::viewport_camera, the one place this decision is made.
 */
struct viewport_camera_pose {
  sbx::math::matrix4x4 world_matrix{sbx::math::matrix4x4::identity};
  sbx::scenes::camera params{};
}; // struct viewport_camera_pose

/**
 * @brief Builds the view/projection matrices for a camera at camera_world_matrix, for a given
 * viewport aspect ratio. Shared by viewport picking and the gizmo (fed from
 * editor_module::viewport_camera) and mirrors what scene_renderer_module::set_camera_override
 * renders with, so all three always agree on what's actually on screen.
 */
[[nodiscard]] auto compute_viewport_camera_matrices(const sbx::math::matrix4x4& camera_world_matrix, const sbx::scenes::camera& camera, std::float_t aspect) -> viewport_camera_matrices;

} // namespace editor

#endif // EDITOR_VIEWPORT_CAMERA_HPP_
