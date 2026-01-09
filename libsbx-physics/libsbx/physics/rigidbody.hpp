#ifndef LIBSBX_PHYSICS_RIGIDBODY_HPP_
#define LIBSBX_PHYSICS_RIGIDBODY_HPP_

#include <cmath>

#include <libsbx/units/mass.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/quaternion.hpp>

namespace sbx::physics {

class rigidbody {

public:

  explicit rigidbody(const units::kilogram& mass = units::kilogram{0.0f});

  // Linear motion
  auto velocity() const -> const math::vector3&;
  auto set_velocity(const math::vector3& velocity) -> void;
  auto add_velocity(const math::vector3& velocity) -> void;

  auto mass() const -> const units::kilogram&;
  auto inverse_mass() const -> std::float_t;
  auto set_mass(const units::kilogram& mass) -> void;
  auto is_static() const -> bool;
  auto make_static() -> void;

  auto add_acceleration(const math::vector3& acceleration) -> void;
  auto add_constant_acceleration(const math::vector3& acceleration) -> void;
  auto set_constant_acceleration(const math::vector3& acceleration) -> void;
  auto clear_constant_forces() -> void;
  auto constant_forces() const -> const math::vector3&;
  auto dynamic_forces() const -> const math::vector3&;
  auto clear_dynamic_forces() -> void;

  // Angular motion
  auto angular_velocity() const -> const math::vector3&;
  auto set_angular_velocity(const math::vector3& angular_velocity) -> void;
  auto add_angular_velocity(const math::vector3& angular_velocity) -> void;

  auto apply_torque(const math::vector3& torque) -> void;
  auto clear_torque() -> void;
  auto torque() const -> const math::vector3&;

  auto apply_angular_impulse(const math::vector3& impulse_world, const math::vector3& contact_vector) -> void;
  auto apply_impulse_at(const math::vector3& impulse_world, const math::vector3& contact_vector) -> void;

  auto inverse_inertia_tensor_world() const -> const math::matrix3x3&;
  auto set_inverse_inertia_tensor_local(const math::matrix3x3& inverse_tensor) -> void;
  auto update_inertia_tensor_world(const math::matrix3x3& rotation) -> void;

private:

  // Linear
  math::vector3 _velocity;
  units::kilogram _mass;
  math::vector3 _constant_forces;
  math::vector3 _dynamic_forces;

  // Angular
  math::vector3 _angular_velocity;
  math::vector3 _torque;
  math::matrix3x3 _inverse_inertia_tensor_local;
  math::matrix3x3 _inverse_inertia_tensor_world;

}; // class rigidbody

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_RIGIDBODY_HPP_
