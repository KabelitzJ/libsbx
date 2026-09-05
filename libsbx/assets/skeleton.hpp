// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASSETS_SKELETON_HPP_
#define LIBSBX_ASSETS_SKELETON_HPP_

#include <cstdint>
#include <string>
#include <vector>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/asset_handle.hpp>

namespace sbx::assets {

/**
 * @brief A joint hierarchy cooked from a glTF skin: parent indices, inverse bind matrices, and
 * bind-pose local TRS.
 *
 * Joints are stored topologically sorted -- a joint's parent_index is always less than its own
 * index -- so evaluating world matrices at runtime is a single forward pass, no recursion needed.
 * Pure CPU data; unlike @ref mesh/@ref texture there's no GPU residency of its own.
 */
class skeleton final {

  friend class asset_residency;

public:

  struct joint {
    std::string name{};
    std::int32_t parent_index{-1}; // -1 = root
    math::matrix4x4 inverse_bind_matrix{math::matrix4x4::identity};
    math::vector3 bind_local_translation{0.0f, 0.0f, 0.0f};
    math::quaternion bind_local_rotation{math::quaternion::identity};
    math::vector3 bind_local_scale{1.0f, 1.0f, 1.0f};
  }; // struct joint

  skeleton() = default;

  explicit skeleton(std::vector<joint> joints)
  : _joints{std::move(joints)} { }

  [[nodiscard]] auto is_valid() const noexcept -> bool {
    return !_joints.empty();
  }

  [[nodiscard]] auto joints() const noexcept -> const std::vector<joint>& {
    return _joints;
  }

  [[nodiscard]] auto id() const noexcept -> const math::uuid& {
    return _id;
  }

private:

  std::vector<joint> _joints{};
  math::uuid _id{math::uuid::nil()};

}; // class skeleton

using skeleton_handle = asset_handle<skeleton>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SKELETON_HPP_
