// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_GIZMO_HPP_
#define LIBSBX_SCENES_COMPONENTS_GIZMO_HPP_

#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp> 

namespace sbx::scenes {

class gizmo final {

public:

  gizmo(math::uuid mesh_id, std::uint32_t submesh_index, math::uuid texture_id, math::color tint = math::color::white())
  : _mesh_id{mesh_id},
    _submesh_index{submesh_index},
    _texture_id{texture_id},
    _tint{tint} { }

  auto mesh_id() const noexcept -> math::uuid {
    return _mesh_id;
  }

  auto submesh_index() const noexcept -> std::uint32_t {
    return _submesh_index;
  }

  auto texture_id() const noexcept -> math::uuid {
    return _texture_id;
  }

  auto tint() const noexcept -> const math::color& {
    return _tint;
  }

  auto set_tint(const math::color& tint) noexcept -> void {
    _tint = tint;
  }

private:

  math::uuid _mesh_id;
  std::uint32_t _submesh_index;
  math::uuid _texture_id;
  math::color _tint;

}; // class gizmo

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_GIZMO_HPP_
