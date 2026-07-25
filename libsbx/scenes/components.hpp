// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_COMPONENTS_HPP_
#define LIBSBX_SCENES_COMPONENTS_HPP_

#include <optional>
#include <vector>

#include <libsbx/math/color.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/ecs/entity.hpp>

#include <libsbx/assets/material.hpp>
#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/texture.hpp>

namespace sbx::scenes {

/**
 * @brief The authored transform of a node, relative to its parent.
 * Its derived from the world transform each frame by scene::update() and written to the world_transform component.
 */
struct local_transform {

  math::vector3f position{0.0f, 0.0f, 0.0f};
  math::quaternion rotation{math::quaternion::identity};
  math::vector3f scale{1.0f, 1.0f, 1.0f};

  [[nodiscard]] auto matrix() const -> math::matrix4x4 {
    const auto translation_matrix = math::matrix4x4::translated(math::matrix4x4::identity, position);
    const auto scale_matrix = math::matrix4x4::scaled(math::matrix4x4::identity, scale);

    return translation_matrix * math::matrix_cast<math::matrix4x4>(rotation) * scale_matrix;
  }
}; // struct local_transform

/**
 * @brief Cached world matrix, written each frame by scene::update().
 */
struct world_transform {
  math::matrix4x4 matrix{math::matrix4x4::identity};
}; // struct world_transform

struct relationship {
  ecs::entity parent{ecs::null_entity};
  std::vector<ecs::entity> children{};
}; // struct relationship

struct id final : math::uuid {

  using base_type = math::uuid;

  id()
  : base_type{} { }

  id(const base_type& base)
  : base_type{base} { }

}; // class id

template<typename Char>
struct basic_tag final : utility::basic_hashed_string<Char> {

  using base_type = utility::basic_hashed_string<Char>;

  template<typename... Args>
  basic_tag(Args&&... args)
  : base_type{std::forward<Args>(args)...} { }

}; // class tag

using tag = basic_tag<char>;

struct camera {
  std::float_t fov_degrees{60.0f};
  std::float_t near_plane{0.1f};
  std::float_t far_plane{1000.0f};
}; // struct camera

/**
 * @brief What a node draws. Holds runtime handles for now; becomes asset uuids in F3.
 */
struct mesh_renderer {
  assets::mesh_handle mesh{};
  std::vector<assets::material_handle> materials{};
}; // struct mesh_renderer

struct directional_light {
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::float_t intensity{1.0f};
}; // struct directional_light

struct point_light {
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::float_t intensity{1.0f};
  std::float_t range{10.0f};
}; // struct point_light

struct spot_light {
  math::color color{1.0f, 1.0f, 1.0f, 1.0f};
  std::float_t intensity{1.0f};
  std::float_t range{10.0f};
  std::float_t inner_angle{0.4f}; // radians
  std::float_t outer_angle{0.6f}; // radians
}; // struct spot_light

} // namespace sbx::scenes

template<typename Char>
struct fmt::formatter<sbx::scenes::basic_tag<Char>> : fmt::formatter<sbx::utility::basic_hashed_string<Char>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  template<typename FormatContext>
  auto format(const sbx::scenes::basic_tag<Char>& tag, FormatContext& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", tag.c_str());
  }

}; // struct fmt::formatter

#endif // LIBSBX_SCENES_COMPONENTS_HPP_
