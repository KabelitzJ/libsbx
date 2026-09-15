// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SHADOW_CASCADE_MATH_HPP_
#define LIBSBX_RENDER_SHADOW_CASCADE_MATH_HPP_

#include <array>
#include <cstdint>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_packet.hpp>

namespace sbx::render {

struct cascade_info {
  math::matrix4x4 view_projection{math::matrix4x4::identity};
  std::float_t split_distance{0.0f}; // View-space (positive, camera-forward) far edge of this cascade's slice.

  // NDC-depth-space equivalent of one shadow-map texel's world-space size in this cascade --
  // (2*radius/shadow_map_resolution) / (this cascade's own near-far depth range, 2*radius +
  // caster_padding). Every cascade shares one texture resolution but a very different world-space
  // area AND a very different depth range (caster_padding=100 alone can dominate a near cascade's
  // small radius), so a single flat NDC bias (tried, reverted -- e.g. Hazel's own 0.002) translates
  // to wildly different world-space slack depending on which cascade and how the scene happens to be
  // scaled: for a small near-cascade radius, 0.002 NDC came out to ~0.24 world units, a quarter of a
  // 1-unit test cube. csm.slang's bias is this * however many texels of slack it wants, giving
  // correctly-scaled world-space bias regardless of scene scale or which cascade.
  std::float_t depth_bias_per_texel{0.0f};
}; // struct cascade_info

/**
 * @brief Splits [camera.near_plane, min(camera.far_plane, shadow_distance)] into shadow_cascade_count
 * slices (a log/uniform blended "practical split" scheme) and builds a texel-snapped light
 * view-projection matrix for each, tightly bounding that slice of the camera frustum.
 *
 * @param camera The active camera this frame.
 * @param aspect The camera's aspect ratio (context.extent.x / context.extent.y).
 * @param light_direction The direction the (sun) light travels, i.e. surface-to-light is -light_direction.
 * @param shadow_distance How far from the camera the cascades should reach; the light's shadow_distance component field.
 */
[[nodiscard]] auto compute_cascades(const camera_data& camera, std::float_t aspect, const math::vector3& light_direction, std::float_t shadow_distance) -> std::array<cascade_info, shadow_cascade_count>;

} // namespace sbx::render

#endif // LIBSBX_RENDER_SHADOW_CASCADE_MATH_HPP_
