#include <libsbx/physics/collider.hpp>

#include <vector>
#include <array>
#include <optional>
#include <limits>
#include <algorithm>
#include <cmath>
#include <map>

namespace sbx::physics {

auto get_local_bounding_volume(const sphere& sphere, const math::vector3& offset) -> math::volume {
  const auto r = math::vector3{sphere.radius};

  return math::volume{offset - r, offset + r};
}

auto get_local_bounding_volume(const box& box, const math::vector3& offset) -> math::volume {
  return math::volume{offset - box.half_extents, offset + box.half_extents};
}

auto get_local_bounding_volume(const cylinder& cylinder, const math::vector3& offset) -> math::volume {
  const auto r = cylinder.radius;

  const auto min = math::vector3{-r, cylinder.base, -r};
  const auto max = math::vector3{r, cylinder.cap, r};

  return math::volume{min + offset, max + offset};
}

auto get_local_bounding_volume(const capsule& capsule, const math::vector3& offset) -> math::volume {
  const auto r = capsule.radius;

  const auto min = math::vector3{-r, capsule.base - r, -r};
  const auto max = math::vector3{r, capsule.cap + r, r};

  return math::volume{min + offset, max + offset};
}

auto get_bounding_volume(const collider& collider, const math::matrix4x4& transform) -> math::volume {
  const auto local_volume = std::visit([&](const auto& shape) {
    return get_local_bounding_volume(shape, collider.offset);
  }, collider.shape);

  return math::volume::transformed(local_volume, transform);
}

} // namespace sbx::physics
