// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/instance_populate.hpp>

#include <algorithm>
#include <random>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector4.hpp>

namespace sbx::scenes {

auto populate_axis_vector(populate_up_axis axis) -> math::vector3 {
  switch (axis) {
    case populate_up_axis::x: return math::vector3{1.0f, 0.0f, 0.0f};
    case populate_up_axis::z: return math::vector3{0.0f, 0.0f, 1.0f};
    case populate_up_axis::y:
    default: return math::vector3{0.0f, 1.0f, 0.0f};
  }
}

// Any unit vector perpendicular to a unit vector.
auto populate_perpendicular(const math::vector3& direction) -> math::vector3 {
  const auto reference = std::abs(direction.y()) < 0.9f ? math::vector3{0.0f, 1.0f, 0.0f} : math::vector3{1.0f, 0.0f, 0.0f};

  return math::vector3::normalized(math::vector3::cross(direction, reference));
}

// The shortest rotation taking unit vector from onto unit vector to.
auto populate_rotation_between(const math::vector3& from, const math::vector3& to) -> math::quaternion {
  const auto cosine = std::clamp(math::vector3::dot(from, to), -1.0f, 1.0f);

  if (cosine > 0.9999f) {
    return math::quaternion::identity;
  }

  if (cosine < -0.9999f) {
    return math::quaternion{populate_perpendicular(from), math::angle{math::radian{math::pif}}};
  }

  return math::quaternion{math::vector3::normalized(math::vector3::cross(from, to)), math::angle{math::radian{std::acos(cosine)}}};
}

auto populate_surface(std::span<const populate_triangle> triangles, const populate_options& options) -> std::vector<instance_data> {
  // Running area totals, so a uniform pick over the total area lands on each triangle in proportion to its size.
  auto areas = std::vector<std::float_t>{};
  auto indices = std::vector<std::size_t>{};
  auto total = 0.0f;

  for (auto index = std::size_t{0u}; index < triangles.size(); ++index) {
    const auto& triangle = triangles[index];
    const auto area = 0.5f * math::vector3::cross(triangle.b - triangle.a, triangle.c - triangle.a).length();

    if (area > 1e-8f) {
      total += area;
      areas.push_back(total);
      indices.push_back(index);
    }
  }

  if (areas.empty() || options.amount == 0u) {
    return {};
  }

  auto random = std::mt19937{options.seed};
  auto unit = std::uniform_real_distribution<std::float_t>{0.0f, 1.0f};
  auto signed_unit = std::uniform_real_distribution<std::float_t>{-1.0f, 1.0f};

  const auto up_axis = populate_axis_vector(options.mesh_up_axis);
  const auto to_radians = math::pif / 180.0f;

  auto instances = std::vector<instance_data>{};
  instances.reserve(options.amount);

  for (auto count = std::uint32_t{0u}; count < options.amount; ++count) {
    const auto pick = unit(random) * total;
    const auto slot = std::min(static_cast<std::size_t>(std::ranges::upper_bound(areas, pick) - areas.begin()), areas.size() - 1u);
    const auto& triangle = triangles[indices[slot]];

    // Folding the square onto the triangle keeps the point uniform over its area.
    auto u = unit(random);
    auto v = unit(random);

    if (u + v > 1.0f) {
      u = 1.0f - u;
      v = 1.0f - v;
    }

    const auto position = triangle.a + (triangle.b - triangle.a) * u + (triangle.c - triangle.a) * v;
    const auto normal = math::vector3::normalized(triangle.normal);

    const auto align = populate_rotation_between(up_axis, normal);
    const auto spin = math::quaternion{normal, math::angle{math::radian{signed_unit(random) * options.random_rotation * to_radians}}};

    const auto tangent = populate_perpendicular(normal);
    const auto bitangent = math::vector3::cross(normal, tangent);
    const auto tilt = math::quaternion{tangent, math::angle{math::radian{signed_unit(random) * options.random_tilt * to_radians}}} * math::quaternion{bitangent, math::angle{math::radian{signed_unit(random) * options.random_tilt * to_radians}}};

    const auto rotation = math::matrix_cast<math::matrix4x4>(tilt * spin * align);
    const auto scale = options.scale * (1.0f + signed_unit(random) * options.random_scale);

    // Rows of the 3x4 transform: row i holds the rotation's row i, scaled, then the position's i-th coordinate.
    auto instance = instance_data{};

    for (auto row = std::size_t{0u}; row < 3u; ++row) {
      instance.rows[row] = math::vector4{rotation[0][row] * scale, rotation[1][row] * scale, rotation[2][row] * scale, position[row]};
    }

    instances.push_back(instance);
  }

  return instances;
}

} // namespace sbx::scenes
