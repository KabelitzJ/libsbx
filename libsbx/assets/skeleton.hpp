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
#include <libsbx/assets/loadable.hpp>

namespace sbx::assets {

/**
 * @brief A joint hierarchy cooked from a glTF skin: parent indices, inverse bind matrices and bind-pose local TRS. CPU data only.
 *
 * Joints are topologically sorted (a parent's index is always lower), so world matrices take a single forward pass.
 */
class skeleton final : public loadable {

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

  // Fills a placeholder skeleton once its cooked content arrives; called once on the main thread.
  auto _finalize_content(std::vector<joint> joints) -> void {
    _joints = std::move(joints);
    _bump_generation();
  }

  std::vector<joint> _joints{};
  math::uuid _id{math::uuid::nil()};

}; // class skeleton

using skeleton_handle = asset_handle<skeleton>;

} // namespace sbx::assets

#endif // LIBSBX_ASSETS_SKELETON_HPP_
