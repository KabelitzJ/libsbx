// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SHADOW_CASCADE_MATH_HPP_
#define LIBSBX_RENDER_SHADOW_CASCADE_MATH_HPP_

#include <array>
#include <cstdint>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_packet.hpp>

namespace sbx::render {

struct cascade_info {
  math::matrix4x4 view_projection{math::matrix4x4::identity};
  std::float_t split_distance{0.0f}; // view-space far edge of this cascade's slice

  // One shadow-map texel's world size in NDC depth units; csm.slang scales its bias by this so it stays correct at any scene scale.
  std::float_t depth_bias_per_texel{0.0f};
  std::float_t texel_world_size{0.0f};

  // World-space sphere (xyz center, w radius) the cascade covers; csm.slang picks the first one containing the point.
  math::vector4 bounding_sphere{};
}; // struct cascade_info

/**
 * @brief Splits the camera range up to the shadow distance into shadow_cascade_count slices (practical split scheme) and builds a texel-snapped light view-projection for each.
 *
 * @param camera The active camera.
 * @param aspect The camera's aspect ratio.
 * @param light_direction The direction the sun's light travels.
 * @param shadow_distance How far from the camera the cascades reach.
 *
 * @return The cascades.
 */
[[nodiscard]] auto compute_cascades(const camera_data& camera, std::float_t aspect, const math::vector3& light_direction, std::float_t shadow_distance) -> std::array<cascade_info, shadow_cascade_count>;

} // namespace sbx::render

#endif // LIBSBX_RENDER_SHADOW_CASCADE_MATH_HPP_
