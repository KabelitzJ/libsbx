// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_CAMERA_HPP_
#define EDITOR_VIEWPORT_CAMERA_HPP_

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/components.hpp>

namespace editor {

struct viewport_camera_matrices {
  sbx::math::matrix4x4 view{sbx::math::matrix4x4::identity};
  sbx::math::matrix4x4 projection{sbx::math::matrix4x4::identity};
}; // struct viewport_camera_matrices

/**
 * @brief Builds the view/projection matrices a camera node renders the viewport with, for a given
 * viewport aspect ratio. Shared by viewport picking and the gizmo so both agree with what's drawn.
 */
[[nodiscard]] auto compute_viewport_camera_matrices(sbx::scenes::node& camera_node, const sbx::scenes::camera& camera, std::float_t aspect) -> viewport_camera_matrices;

} // namespace editor

#endif // EDITOR_VIEWPORT_CAMERA_HPP_
