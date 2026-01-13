#ifndef LIBSBX_PHYSICS_COLLISION_DETECTION_HPP_
#define LIBSBX_PHYSICS_COLLISION_DETECTION_HPP_

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/physics/collider.hpp>

namespace sbx::physics {

struct collision_manifold {
  math::vector3 normal;
  std::float_t depth;
}; // struct collision_manifold

auto check_collision(const collider& c_a, const math::vector3& p_a, const math::matrix3x3& rs_a, const collider& c_b, const math::vector3& p_b, const math::matrix3x3& rs_b) -> std::optional<collision_manifold>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLISION_DETECTION_HPP_