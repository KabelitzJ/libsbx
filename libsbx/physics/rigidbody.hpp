// SPDX-License-Identifier: MIT
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

  rigidbody(const std::float_t mass = 1.0f) {
    set_mass(mass);
  }

  auto mass() const noexcept -> std::float_t {
    if (_inverse_mass <= std::numeric_limits<std::float_t>::epsilon()) {
      return std::numeric_limits<std::float_t>::infinity();
    }

    return 1.0f / _inverse_mass;
  }

  auto set_mass(const std::float_t mass) noexcept -> void {
    if (mass <= std::numeric_limits<std::float_t>::epsilon()) {
      _inverse_mass = 0.0f;
      _is_static = true;
  
      _inverse_inertia_tensor = math::matrix3x3::zero; 
      _inverse_inertia_tensor_world = math::matrix3x3::zero;
    } else {
      _inverse_mass = 1.0f / mass;
      _is_static = false;
    }
  }

  auto inverse_mass() const noexcept -> std::float_t {
    return _inverse_mass;
  }

  auto is_static() const noexcept -> bool {
    return _is_static;
  }

  auto set_is_static(const bool is_static) noexcept -> void {
    _is_static = is_static;

    if (_is_static) {
      _inverse_mass = 0.0f;
      _velocity = math::vector3::zero;
      _angular_velocity = math::vector3::zero;
      _inverse_inertia_tensor = math::matrix3x3::zero;
      _inverse_inertia_tensor_world = math::matrix3x3::zero;
    }
  }

  auto linear_damping() const noexcept -> std::float_t {
    return _linear_damping;
  }

  auto set_linear_damping(std::float_t damping) noexcept -> void {
    _linear_damping = damping;
  }

  auto angular_damping() const noexcept -> std::float_t {
    return _angular_damping;
  }

  auto set_angular_damping(std::float_t damping) noexcept -> void {
    _angular_damping = damping;
  }

  auto velocity() const noexcept -> const math::vector3& {
    return _velocity;
  }

  auto set_velocity(const math::vector3& velocity) noexcept -> void {
    _velocity = velocity;
  }

  auto add_velocity(const math::vector3& delta) noexcept -> void {
    _velocity += delta;
  }

  auto angular_velocity() const noexcept -> const math::vector3& {
    return _angular_velocity;
  }

  auto set_angular_velocity(const math::vector3& velocity) noexcept -> void {
    _angular_velocity = velocity;
  }

  auto add_angular_velocity(const math::vector3& delta) noexcept -> void {
    _angular_velocity += delta;
  }

  auto add_force(const math::vector3& force) noexcept -> void {
    _force_accumulator += force;
  }

  auto add_force_at_point(const math::vector3& force, const math::vector3& point, const math::vector3& center_of_mass) noexcept -> void {
    const auto arm = point - center_of_mass;
    _force_accumulator += force;
    _torque_accumulator += math::vector3::cross(arm, force);
  }

  auto constant_forces() const noexcept -> const math::vector3& {
    return _constant_forces;
  }
  
  auto set_constant_forces(const math::vector3& forces) noexcept -> void {
    _constant_forces = forces;
  }

  auto add_constant_forces(const math::vector3& forces) noexcept -> void {
    _constant_forces += forces;
  }

  auto add_acceleration(const math::vector3& acceleration) noexcept -> void {
    if (_is_static) {
      return;
    }
    
    const auto m = 1.0f / _inverse_mass;
  
    _force_accumulator += acceleration * m;
  }

  auto add_constant_acceleration(const math::vector3& acceleration) noexcept -> void {
    if (_is_static) {
      return;
    }
    
    const auto m = 1.0f / _inverse_mass;

    _constant_forces += acceleration * m;
  }

  auto set_constant_acceleration(const math::vector3& acceleration) noexcept -> void {
    if (_is_static) {
      _constant_forces = math::vector3::zero;
      return;
    }
    
    const auto m = 1.0f / _inverse_mass;

    _constant_forces = acceleration * m;
  }

  auto dynamic_forces() const noexcept -> const math::vector3& {
    return _force_accumulator;
  }

  auto clear_dynamic_forces() noexcept -> void {
    _force_accumulator = math::vector3::zero;
  }

  auto add_torque(const math::vector3& torque) noexcept -> void {
    _torque_accumulator += torque;
  }

  auto torque() const noexcept -> const math::vector3& {
    return _torque_accumulator;
  }

  auto clear_torque() noexcept -> void {
    _torque_accumulator = math::vector3::zero;
  }

  auto inverse_inertia_tensor() const noexcept -> const math::matrix3x3& {
    return _inverse_inertia_tensor;
  }

  auto set_inverse_inertia_tensor(const math::matrix3x3& inverse_tensor) noexcept -> void {
    if (_is_static) {
      _inverse_inertia_tensor = math::matrix3x3::zero;
    } else {
      _inverse_inertia_tensor = inverse_tensor;
    }
  }

  auto inverse_inertia_tensor_world() const noexcept -> const math::matrix3x3& {
    return _inverse_inertia_tensor_world;
  }

  auto update_inertia_tensor_world(const math::matrix3x3& rotation) noexcept -> void {
    _inverse_inertia_tensor_world = rotation * _inverse_inertia_tensor * math::matrix3x3::transposed(rotation);
  }

private:

  std::float_t _inverse_mass{1.0f};
  bool _is_static{false};

  std::float_t _linear_damping{0.01f};
  std::float_t _angular_damping{0.05f};

  math::vector3 _velocity{math::vector3::zero};
  math::vector3 _angular_velocity{math::vector3::zero};

  math::vector3 _force_accumulator{math::vector3::zero};
  math::vector3 _torque_accumulator{math::vector3::zero};
  
  math::vector3 _constant_forces{math::vector3::zero}; 

  math::matrix3x3 _inverse_inertia_tensor{math::matrix3x3::zero};
  math::matrix3x3 _inverse_inertia_tensor_world{math::matrix3x3::zero};

}; // class rigidbody

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_RIGIDBODY_HPP_
