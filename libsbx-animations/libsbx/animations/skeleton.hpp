// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_SKELETON_HPP_
#define LIBSBX_ANIMATIONS_SKELETON_HPP_

#include <string>
#include <cstdint>
#include <unordered_map>
#include <cmath>
#include <optional>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hashed_string.hpp>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/animations/animation.hpp>

namespace sbx::animations {

class skeleton {

public:

  struct bone {
    inline static constexpr auto null = std::uint32_t{0xFFFFFFFF};

    std::uint32_t parent_id;
    math::matrix4x4 local_bind_matrix;
    math::matrix4x4 inverse_bind_matrix;
  }; // struct bone

  inline static constexpr auto max_bones = std::uint32_t{64u};

  skeleton() = default;

  auto reserve(const std::size_t size) -> void;

  auto shrink_to_fit() -> void;

  auto add_bone(const std::string& name, const bone& bone) -> void;

  auto root_transform() const -> const math::matrix4x4&;

  auto inverse_root_transform() const -> const math::matrix4x4&;
  
  auto set_inverse_root_transform(const math::matrix4x4& inverse_root_transform) -> void;

  auto bones() const -> const memory::vector<bone>&;

  auto evaluate_pose(const animation& animation, std::float_t time) const -> memory::vector<math::matrix4x4>;

  auto bone_count() const -> std::uint32_t;

  auto name_for_bone(const std::size_t index) const -> const utility::hashed_string&;

  auto bone_index(const utility::hashed_string& name) const -> std::optional<std::uint32_t>;

private:

  memory::vector<bone> _bones;
  memory::vector<utility::hashed_string> _bone_names_by_id;
  memory::unordered_map<utility::hashed_string, std::uint32_t> _bone_id_by_name;
  
  math::matrix4x4 _inverse_root_transform;
  math::matrix4x4 _root_transform;

}; // class skeleton

} // namespace sbx::animation

#endif // LIBSBX_ANIMATIONS_SKELETON_HPP_
