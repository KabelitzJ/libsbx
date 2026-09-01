// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/shadow/cascade.hpp>

#include <algorithm>
#include <cmath>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/vector4.hpp>

namespace sbx::render {

inline constexpr auto cascade_lambda = 0.85f;

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

auto compute_cascades(const camera_data& camera, std::float_t aspect, const math::vector3f& light_direction, std::float_t shadow_distance) -> std::array<cascade_info, shadow_cascade_count> {
  const auto near_plane = camera.near_plane;
  const auto far_plane = std::min(camera.far_plane, shadow_distance);

  const auto splits = compute_splits(near_plane, far_plane);

  const auto camera_world = math::matrix4x4::inverted(camera.view);
  const auto camera_forward = math::vector3f::normalized(math::vector3f{-camera_world[2].x(), -camera_world[2].y(), -camera_world[2].z()});

  const auto fov_y = math::to_radians(math::degree{camera.fov_degrees}).value();
  const auto tan_half_y = std::tan(fov_y * 0.5f);
  const auto tan_half_x = tan_half_y * aspect;
  const auto tan_sq = tan_half_x * tan_half_x + tan_half_y * tan_half_y;

  const auto light_dir = math::vector3f::normalized(light_direction);

  auto up = math::vector3f{0.0f, 1.0f, 0.0f};

  if (std::abs(math::vector3f::dot(light_dir, up)) > 0.99f) {
    up = math::vector3f{1.0f, 0.0f, 0.0f};
  }

  constexpr auto caster_padding = 100.0f;

  auto result = std::array<cascade_info, shadow_cascade_count>{};

  auto slice_near = near_plane;

  for (auto i = std::uint32_t{0u}; i < shadow_cascade_count; ++i) {
    const auto slice_far = splits[i];

    const auto half_length = (slice_far - slice_near) * 0.5f;
    const auto radius = std::sqrt(half_length * half_length + slice_far * slice_far * tan_sq);

    const auto center_world = camera.position + camera_forward * ((slice_near + slice_far) * 0.5f);

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

    result[i].view_projection = shadow_matrix;
    result[i].split_distance = slice_far;

    slice_near = slice_far;
  }

  return result;
}

} // namespace sbx::render
