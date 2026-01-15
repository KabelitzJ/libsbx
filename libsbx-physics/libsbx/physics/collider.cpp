#include <libsbx/physics/collider.hpp>

#include <vector>
#include <array>
#include <optional>
#include <limits>
#include <algorithm>
#include <cmath>
#include <map>

namespace sbx::physics {

auto inverse_inertia_tensor(const sphere& s, std::float_t mass) -> math::matrix3x3 {
  const auto i = 0.4f * mass * s.radius * s.radius;

  if (i < std::numeric_limits<std::float_t>::epsilon()) {
    return math::matrix3x3::zero;
  }
  
  const auto inv_i = 1.0f / i;

  return math::matrix3x3{inv_i, inv_i, inv_i};
}

auto inverse_inertia_tensor(const box& b, std::float_t mass) -> math::matrix3x3 {
  if (mass < std::numeric_limits<std::float_t>::epsilon()) {
    return math::matrix3x3::zero;
  }

  const auto w = b.half_extents.x() * 2.0f;
  const auto h = b.half_extents.y() * 2.0f;
  const auto d = b.half_extents.z() * 2.0f;
  
  const auto factor = (1.0f / 12.0f) * mass;
  
  const auto i_x = factor * (h * h + d * d);
  const auto i_y = factor * (w * w + d * d);
  const auto i_z = factor * (w * w + h * h);

  return math::matrix3x3{
    (i_x > 0.0f) ? 1.0f / i_x : 0.0f,
    (i_y > 0.0f) ? 1.0f / i_y : 0.0f,
    (i_z > 0.0f) ? 1.0f / i_z : 0.0f
  };
}

auto inverse_inertia_tensor(const cylinder& c, std::float_t mass) -> math::matrix3x3 {
  if (mass < std::numeric_limits<std::float_t>::epsilon()) {
    return math::matrix3x3::zero;
  }

  const auto r2 = c.radius * c.radius;
  const auto h = 2.0f * c.half_height;
  const auto h2 = h * h;

  const auto i_y = 0.5f * mass * r2;
  const auto i_xz = (1.0f / 12.0f) * mass * (3.0f * r2 + h2);

  return math::matrix3x3{
    (i_xz > 0.0f) ? 1.0f / i_xz : 0.0f,
    (i_y  > 0.0f) ? 1.0f / i_y  : 0.0f,
    (i_xz > 0.0f) ? 1.0f / i_xz : 0.0f
  };
}

auto inverse_inertia_tensor(const capsule& c, std::float_t mass) -> math::matrix3x3 {
  const auto height = (2.0f * c.half_height) + (2.0f * c.radius);

  if (mass < std::numeric_limits<std::float_t>::epsilon()) {
    return math::matrix3x3::zero;
  }

  const auto r2 = c.radius * c.radius;
  const auto h2 = height * height;

  const auto i_y = 0.5f * mass * r2;
  const auto i_xz = (1.0f / 12.0f) * mass * (3.0f * r2 + h2);

  return math::matrix3x3{
    (i_xz > 0.0f) ? 1.0f / i_xz : 0.0f,
    (i_y  > 0.0f) ? 1.0f / i_y  : 0.0f,
    (i_xz > 0.0f) ? 1.0f / i_xz : 0.0f
  };
}

auto inverse_inertia_tensor(const collider& collider, std::float_t mass) -> math::matrix3x3 {
  return std::visit([mass](const auto& shape){
    return inverse_inertia_tensor(shape, mass);
  }, collider.shape);
}

auto get_local_bounding_volume(const box& box, const math::vector3& offset) -> math::volume {
  return math::volume{offset - box.half_extents, offset + box.half_extents};
}

auto get_local_bounding_volume(const sphere& sphere, const math::vector3& offset) -> math::volume {
  const auto r = math::vector3{sphere.radius};

  return math::volume{offset - r, offset + r};
}

auto get_local_bounding_volume(const cylinder& cylinder, const math::vector3& offset) -> math::volume {
  const auto r = cylinder.radius;

  const auto min = math::vector3{-r, -cylinder.half_height, -r};
  const auto max = math::vector3{r, cylinder.half_height, r};

  return math::volume{min + offset, max + offset};
}

auto get_local_bounding_volume(const capsule& capsule, const math::vector3& offset) -> math::volume {
  const auto r = capsule.radius;

  const auto min = math::vector3{-r, -capsule.half_height - r, -r};
  const auto max = math::vector3{r, capsule.half_height + r, r};

  return math::volume{min + offset, max + offset};
}

auto get_bounding_volume(const collider& collider, const math::matrix4x4& transform) -> math::volume {
  const auto local_volume = std::visit([&](const auto& shape) {
    return get_local_bounding_volume(shape, collider.offset);
  }, collider.shape);

  return math::volume::transformed(local_volume, transform);
}

} // namespace sbx::physics
