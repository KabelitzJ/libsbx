#ifndef LIBSBX_PHYSICS_INERTIA_HPP_
#define LIBSBX_PHYSICS_INERTIA_HPP_

#include <libsbx/math/matrix3x3.hpp>

#include <libsbx/physics/collider.hpp>

namespace sbx::physics {

struct inertia {

  // Sphere Inertia: I = 2/5 * m * r^2
  // Inverse: 5/2 * 1/(m * r^2)
  static auto inverse_tensor(const sphere& s, std::float_t mass) -> math::matrix3x3 {
    const auto i = 0.4f * mass * s.radius * s.radius;

    if (i < std::numeric_limits<std::float_t>::epsilon()) {
      return math::matrix3x3::zero;
    }
    
    const auto inv_i = 1.0f / i;

    return math::matrix3x3{inv_i, inv_i, inv_i};
  }

  // Box Inertia: I_xx = m/12 * (h^2 + d^2) ...
  // Inverse: 12/m * 1/(h^2 + d^2) ...
  static auto inverse_tensor(const box& b, std::float_t mass) -> math::matrix3x3 {
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

  // Cylinder Inertia (Y-axis aligned)
  // I_y = 1/2 * m * r^2
  // I_x = I_z = 1/12 * m * (3*r^2 + h^2)
  static auto inverse_tensor(const cylinder& c, std::float_t mass) -> math::matrix3x3 {
    if (mass < std::numeric_limits<std::float_t>::epsilon()) {
      return math::matrix3x3::zero;
    }

    const auto r2 = c.radius * c.radius;
    const auto h = std::abs(c.cap - c.base);
    const auto h2 = h * h;

    const auto i_y = 0.5f * mass * r2;
    const auto i_xz = (1.0f / 12.0f) * mass * (3.0f * r2 + h2);

    return math::matrix3x3{
      (i_xz > 0.0f) ? 1.0f / i_xz : 0.0f,
      (i_y  > 0.0f) ? 1.0f / i_y  : 0.0f,
      (i_xz > 0.0f) ? 1.0f / i_xz : 0.0f
    };
  }

  // Capsule Inertia 
  // Approximated as a cylinder for performance, or use bounding cylinder
  static auto inverse_tensor(const capsule& c, std::float_t mass) -> math::matrix3x3 {
    const auto height = std::abs(c.cap - c.base) + 2.0f * c.radius;

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

  static auto inverse_tensor(const collider& collider, std::float_t mass) -> math::matrix3x3 {
    return std::visit([mass](const auto& shape){
      return inverse_tensor(shape, mass);
    }, collider.shape);
  }

}; // struct inertia

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_INERTIA_HPP_