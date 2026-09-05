// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/particles/spawn.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

#include <libsbx/math/random.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector4.hpp>

namespace sbx::particles {

[[nodiscard]] auto sample_local_position(const assets::particle_emitter& config) -> math::vector3 {
  switch (config.shape) {
    case assets::emitter_shape::sphere: {
      return math::random_point_in_sphere(math::vector3{0.0f, 0.0f, 0.0f}, config.shape_extents.x());
    }
    case assets::emitter_shape::box: {
      const auto& half_extents = config.shape_extents;

      return math::vector3{
        math::random::next<std::float_t>(-half_extents.x(), half_extents.x()),
        math::random::next<std::float_t>(-half_extents.y(), half_extents.y()),
        math::random::next<std::float_t>(-half_extents.z(), half_extents.z())
      };
    }
    case assets::emitter_shape::cone: {
      const auto half_angle = std::max(config.cone.angle.to_radians().value(), 0.001f);
      const auto height = config.cone.radius / std::tan(half_angle);

      const auto t = (math::random::next<std::float_t>(0.0f, 1.0f) < config.cone.emit_from_volume) ? std::cbrt(math::random::next<std::float_t>(0.0f, 1.0f)) : 1.0f;

      const auto disc = math::random_point_in_circle(math::vector2{0.0f, 0.0f}, config.cone.radius * t);

      return math::vector3{disc.x(), disc.y(), -height * t};
    }
    default: {
      return math::vector3{0.0f, 0.0f, 0.0f};
    }
  }
}

auto roll_particle(const assets::particle_emitter& config, const math::matrix4x4& world, std::uint32_t id) -> particle {
  const auto local_position = sample_local_position(config);

  const auto local_velocity = math::vector3{
    math::random::next<std::float_t>(config.velocity_min.x(), config.velocity_max.x()),
    math::random::next<std::float_t>(config.velocity_min.y(), config.velocity_max.y()),
    math::random::next<std::float_t>(config.velocity_min.z(), config.velocity_max.z())
  };

  auto result = particle{};

  result.id = id;
  result.position = math::vector3{world * math::vector4{local_position, 1.0f}};
  result.velocity = math::vector3{world * math::vector4{local_velocity, 0.0f}};
  result.rotation = math::random::next<std::float_t>(config.rotation_min, config.rotation_max);
  result.age = 0.0f;
  result.lifetime = math::random::next<std::float_t>(config.lifetime_min, config.lifetime_max);
  result.base_size = math::random::next<std::float_t>(config.size_min, config.size_max);
  result.size = result.base_size;
  result.color = config.start_color;

  return result;
}

} // namespace sbx::particles
