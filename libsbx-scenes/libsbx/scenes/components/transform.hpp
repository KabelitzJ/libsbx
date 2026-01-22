// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_TRANSFORM_HPP_
#define LIBSBX_SCENES_COMPONENTS_TRANSFORM_HPP_

#include <numbers>

#include <libsbx/ecs/meta.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/angle.hpp>
#include <libsbx/math/quaternion.hpp>

namespace sbx::scenes {

class transform final {

public:

  transform(const math::vector3& position = math::vector3::zero, const math::quaternion& rotation = math::quaternion::identity, const math::vector3& scale = math::vector3::one);

  ~transform() = default;

  auto position() const noexcept -> const math::vector3&;

  auto position() noexcept -> math::vector3&;

  auto set_position(const math::vector3& position) noexcept -> void;

  auto move_by(const math::vector3& offset) noexcept -> void;

  auto rotation() const noexcept -> const math::quaternion&;

  auto set_rotation(const math::quaternion& rotation) noexcept -> void;

  auto set_rotation(const math::vector3& axis, const math::angle& angle) noexcept -> void;

  auto scale() noexcept -> math::vector3&;

  auto scale() const noexcept -> const math::vector3&;

  auto set_scale(const math::vector3& scale) noexcept -> void;

  auto forward() const noexcept -> math::vector3;

  auto right() const noexcept -> math::vector3;

  auto up() const noexcept -> math::vector3;

  auto look_at(const math::vector3& target) noexcept -> void;

  auto version() const noexcept -> std::uint64_t;

  auto bump_version() -> void;

  [[nodiscard]] auto local_transform() const -> math::matrix4x4;

private:

  math::vector3 _position;
  math::quaternion _rotation;
  math::vector3 _scale;

  math::matrix4x4 _rotation_matrix;

  std::uint64_t _version;

}; // class transform

} // namespace sbx::scenes

// template<>
// struct sbx::ecs::meta<sbx::scenes::transform> {
//   auto operator()(const utility::hashed_string& tag, sbx::scenes::transform& value) -> void {
//     if (tag == "save") {
      
//     }
//   }
// }; // sbx::ecs::meta

#endif // LIBSBX_SCENES_COMPONENTS_TRANSFORM_HPP_
