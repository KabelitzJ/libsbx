// SPDX-License-Identifier: MIT
#include <libsbx/scenes/components/transform.hpp>

#include <libsbx/math/matrix_cast.hpp>

namespace sbx::scenes {

transform::transform(const math::vector3& position, const math::quaternion& rotation, const math::vector3& scale)
: _position{position}, 
  _rotation{rotation}, 
  _scale{scale},
  _rotation_matrix{math::matrix_cast<math::matrix4x4>(_rotation)},
  _version{1u} { }

auto transform::position() const noexcept -> const math::vector3& {
  return _position;
}

auto transform::position() noexcept -> math::vector3& {
  bump_version();
  return _position;
}

auto transform::set_position(const math::vector3& position) noexcept -> void {
  _position = position;
  bump_version();
}

auto transform::move_by(const math::vector3& offset) noexcept -> void {
  _position += offset;
  bump_version();
}

auto transform::rotation() const noexcept -> const math::quaternion& {
  return _rotation;
}

auto transform::set_rotation(const math::quaternion& rotation) noexcept -> void {
  _rotation = rotation;
  _rotation_matrix = math::matrix_cast<math::matrix4x4>(_rotation);
  bump_version();
}

auto transform::set_rotation(const math::vector3& axis, const math::angle& angle) noexcept -> void {
  _rotation = math::quaternion{axis, angle};
  _rotation_matrix = math::matrix_cast<math::matrix4x4>(_rotation);
  bump_version();
}

auto transform::scale() noexcept -> math::vector3& {
  bump_version();
  return _scale;
}

auto transform::scale() const noexcept -> const math::vector3& {
  return _scale;
}


auto transform::set_scale(const math::vector3& scale) noexcept -> void {
  _scale = scale;
  bump_version();
}

auto transform::forward() const noexcept -> math::vector3 {
  return -math::vector3{_rotation_matrix[2]};
}

auto transform::right() const noexcept -> math::vector3 {
  return math::vector3{_rotation_matrix[0]};
}

auto transform::up() const noexcept -> math::vector3 {
  return math::vector3{_rotation_matrix[1]};
}

auto transform::look_at(const math::vector3& target) noexcept -> void {
  // [TODO] : Figure out how to directly construct the rotation_matrix
  auto result = math::matrix4x4::look_at(_position, target, math::vector3::up);
  _rotation = math::quaternion{math::matrix4x4::inverted(result)};
  _rotation_matrix = math::matrix_cast<math::matrix4x4>(_rotation);
  bump_version();
}

auto transform::version() const noexcept -> std::uint64_t {
  return _version;
}

auto transform::bump_version() -> void {
  ++_version;
}

auto transform::local_transform() const -> math::matrix4x4 {
  const auto translation = math::matrix4x4::translated(math::matrix4x4::identity, _position);
  const auto scale = math::matrix4x4::scaled(math::matrix4x4::identity, _scale);

  return translation * _rotation_matrix * scale;
}

} // namespace sbx::scenes