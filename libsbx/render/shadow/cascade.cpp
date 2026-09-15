// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/shadow/cascade.hpp>

#include <algorithm>
#include <cmath>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/vector4.hpp>

namespace sbx::render {

inline constexpr auto cascade_lambda = 0.85f;

// Must match shaders/shadows/csm.slang's own cascade_blend_threshold exactly -- see this loop's use
// of it below for why.
inline constexpr auto cascade_blend_threshold = 0.9f;

[[nodiscard]] auto lerp_float(std::float_t a, std::float_t b, std::float_t t) noexcept -> std::float_t {
  return a + (b - a) * t;
}

[[nodiscard]] auto compute_splits(std::float_t near_plane, std::float_t far_plane) -> std::array<std::float_t, shadow_cascade_count> {
  auto splits = std::array<std::float_t, shadow_cascade_count>{};

  for (auto i = std::uint32_t{0u}; i < shadow_cascade_count; ++i) {
    const auto p = static_cast<std::float_t>(i + 1u) / static_cast<std::float_t>(shadow_cascade_count);

    const auto log_split = near_plane * std::pow(far_plane / near_plane, p);
    const auto uniform_split = near_plane + (far_plane - near_plane) * p;

    splits[i] = lerp_float(uniform_split, log_split, cascade_lambda);
  }

  return splits;
}

auto compute_cascades(const camera_data& camera, std::float_t aspect, const math::vector3& light_direction, std::float_t shadow_distance) -> std::array<cascade_info, shadow_cascade_count> {
  const auto near_plane = std::max(camera.near_plane, 0.5f);
  const auto far_plane = std::min(camera.far_plane, shadow_distance);

  const auto splits = compute_splits(near_plane, far_plane);

  const auto camera_world = math::matrix4x4::inverted(camera.view);
  const auto camera_forward = math::vector3::normalized(math::vector3{-camera_world[2].x(), -camera_world[2].y(), -camera_world[2].z()});

  const auto fov_y = math::to_radians(math::degree{camera.fov_degrees}).value();
  const auto tan_half_y = std::tan(fov_y * 0.5f);
  const auto tan_half_x = tan_half_y * aspect;
  const auto tan_sq = tan_half_x * tan_half_x + tan_half_y * tan_half_y;

  const auto light_dir = math::vector3::normalized(light_direction);

  auto up = math::vector3{0.0f, 1.0f, 0.0f};

  if (std::abs(math::vector3::dot(light_dir, up)) > 0.99f) {
    up = math::vector3{1.0f, 0.0f, 0.0f};
  }

  constexpr auto caster_padding = 100.0f;

  auto result = std::array<cascade_info, shadow_cascade_count>{};

  auto slice_near = near_plane;

  for (auto i = std::uint32_t{0u}; i < shadow_cascade_count; ++i) {
    const auto slice_far = splits[i];

    // csm.slang's calculate_shadow blends cascade i into cascade i+1 starting at view_depth ==
    // splits[i] * cascade_blend_threshold (still classified as cascade i, but already sampling
    // cascade i+1 too) -- so cascade i+1's own frustum, built here, has to reach back past its
    // nominal slice_near (== splits[i]) to actually cover that zone. Without this, that blend
    // samples cascade i+1's shadow map for a point just outside its tightly-fit frustum,
    // sample_cascade_pcf's out-of-bounds check returns 1.0 (fully lit), and the blend punches an
    // unshaded line straight through the shadow exactly where the two cascades meet. Cascade 0 has
    // no previous cascade blending into it, so it keeps its own tight near bound.
    const auto frustum_near = i == 0u ? slice_near : slice_near * cascade_blend_threshold;

    const auto half_length = (slice_far - frustum_near) * 0.5f;
    const auto radius = std::sqrt(half_length * half_length + slice_far * slice_far * tan_sq);

    const auto center_world = camera.position + camera_forward * ((frustum_near + slice_far) * 0.5f);

    const auto light_position = center_world - light_dir * (radius + caster_padding);
    const auto light_view = math::matrix4x4::look_at(light_position, center_world, up);

    const auto light_projection = math::matrix4x4::orthographic(-radius, radius, -radius, radius, 0.0f, 2.0f * radius + caster_padding);

    auto shadow_matrix = light_projection * light_view;

    const auto resolution = static_cast<std::float_t>(shadow_map_resolution);
    const auto shadow_origin = (shadow_matrix * math::vector4{0.0f, 0.0f, 0.0f, 1.0f}) * (resolution * 0.5f);

    const auto rounded_x = std::round(shadow_origin.x());
    const auto rounded_y = std::round(shadow_origin.y());

    const auto offset_x = (rounded_x - shadow_origin.x()) * (2.0f / resolution);
    const auto offset_y = (rounded_y - shadow_origin.y()) * (2.0f / resolution);

    shadow_matrix[3].x() += offset_x;
    shadow_matrix[3].y() += offset_y;

    const auto depth_range = 2.0f * radius + caster_padding; // matches the orthographic() near/far span above
    const auto texel_world_size = (2.0f * radius) / resolution;

    result[i].view_projection = shadow_matrix;
    result[i].split_distance = slice_far;
    result[i].depth_bias_per_texel = texel_world_size / depth_range;

    slice_near = slice_far;
  }

  return result;
}

} // namespace sbx::render
