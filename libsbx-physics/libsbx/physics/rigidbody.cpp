#include <libsbx/physics/rigidbody.hpp>

#include <libsbx/math/matrix_cast.hpp>

namespace sbx::physics {

rigidbody::rigidbody(const units::kilogram& mass)
: _velocity{math::vector3::zero},
  _mass{mass},
  _constant_forces{math::vector3::zero},
  _dynamic_forces{math::vector3::zero},
  _angular_velocity{math::vector3::zero},
  _torque{math::vector3::zero},
  _inverse_inertia_tensor_local{math::matrix3x3::zero},
  _inverse_inertia_tensor_world{math::matrix3x3::zero} { }

auto rigidbody::velocity() const -> const math::vector3& {
  return _velocity;
}

auto rigidbody::set_velocity(const math::vector3& velocity) -> void {
  _velocity = (velocity.length_squared() < 1e-6f) ? math::vector3::zero : velocity;
}

auto rigidbody::add_velocity(const math::vector3& velocity) -> void {
  _velocity += (velocity.length_squared() < 1e-6f) ? math::vector3::zero : velocity;
}

auto rigidbody::mass() const -> const units::kilogram& {
  return _mass;
}

auto rigidbody::inverse_mass() const -> std::float_t {
  return is_static() ? 0.0f : 1.0f / _mass.value();
}

auto rigidbody::set_mass(const units::kilogram& mass) -> void {
  _mass = mass;
}

auto rigidbody::is_static() const -> bool {
  return _mass == units::kilogram{0.0f};
}

auto rigidbody::make_static() -> void {
  set_mass(units::kilogram{0.0f});
}

auto rigidbody::add_acceleration(const math::vector3& acceleration) -> void {
  _dynamic_forces += acceleration * _mass;
}

auto rigidbody::add_constant_acceleration(const math::vector3& acceleration) -> void {
  _constant_forces += acceleration * _mass;
}

auto rigidbody::set_constant_acceleration(const math::vector3& acceleration) -> void {
  _constant_forces = acceleration * _mass;
}

auto rigidbody::clear_constant_forces() -> void {
  _constant_forces = math::vector3::zero;
}

auto rigidbody::constant_forces() const -> const math::vector3& {
  return _constant_forces;
}

auto rigidbody::dynamic_forces() const -> const math::vector3& {
  return _dynamic_forces;
}

auto rigidbody::clear_dynamic_forces() -> void {
  _dynamic_forces = math::vector3::zero;
}

// === ANGULAR ===

auto rigidbody::angular_velocity() const -> const math::vector3& {
  return _angular_velocity;
}

auto rigidbody::set_angular_velocity(const math::vector3& angular_velocity) -> void {
  _angular_velocity = (angular_velocity.length_squared() < 1e-6f) ? math::vector3::zero : angular_velocity;
}

auto rigidbody::add_angular_velocity(const math::vector3& angular_velocity) -> void {
  _angular_velocity += (angular_velocity.length_squared() < 1e-6f) ? math::vector3::zero : angular_velocity;
}

auto rigidbody::apply_torque(const math::vector3& torque) -> void {
  _torque += torque;
}

auto rigidbody::clear_torque() -> void {
  _torque = math::vector3::zero;
}

auto rigidbody::torque() const -> const math::vector3& {
  return _torque;
}

// === INERTIA TENSOR ===

auto rigidbody::set_inverse_inertia_tensor_local(const math::matrix3x3& inverse_tensor) -> void {
  _inverse_inertia_tensor_local = inverse_tensor;
}

auto rigidbody::inverse_inertia_tensor_world() const -> const math::matrix3x3& {
  return _inverse_inertia_tensor_world;
}

auto rigidbody::update_inertia_tensor_world(const math::matrix3x3& rotation) -> void {
  _inverse_inertia_tensor_world = rotation * _inverse_inertia_tensor_local * math::matrix3x3::transposed(rotation);
}

// === ANGULAR IMPULSE ===

auto rigidbody::apply_angular_impulse(const math::vector3& impulse_world, const math::vector3& contact_vector) -> void {
  const auto angular_impulse = math::vector3::cross(contact_vector, impulse_world);

  _angular_velocity += _inverse_inertia_tensor_world * angular_impulse;
}

auto rigidbody::apply_impulse_at(const math::vector3& impulse_world, const math::vector3& contact_vector) -> void {
  // Linear v
  _velocity += impulse_world * inverse_mass();

  // Angular v: τ = r × J
  const auto torque_impulse = math::vector3::cross(contact_vector, impulse_world);
  _angular_velocity += _inverse_inertia_tensor_world * torque_impulse;
}

} // namespace sbx::physics
