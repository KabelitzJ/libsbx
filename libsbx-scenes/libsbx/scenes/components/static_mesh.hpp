// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_COMPONENTS_STATIC_MESH_HPP_
#define LIBSBX_SCENES_COMPONENTS_STATIC_MESH_HPP_

#include <vector>
#include <cinttypes>

#include <libsbx/math/color.hpp>
#include <libsbx/math/uuid.hpp> 

#include <libsbx/graphics/resource_storage.hpp>

#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::scenes {

class static_mesh final {

public:

  struct submesh {
    std::uint32_t index;
    math::uuid material;
  }; // struct submesh

  static_mesh(const math::uuid mesh_id, const math::uuid material)
  : _mesh_id{mesh_id},
    _submeshes{{0, material}} { }

  static_mesh(const math::uuid mesh_id, const std::vector<submesh>& submeshes)
  : _mesh_id{mesh_id},
    _submeshes{submeshes} { }

  static_mesh(const math::uuid mesh_id, std::initializer_list<submesh> submeshes)
  : _mesh_id{mesh_id},
    _submeshes{submeshes} { }

  auto mesh_id() const noexcept -> math::uuid {
    return _mesh_id;
  }

  auto submeshes() const noexcept -> const std::vector<submesh>& {
    return _submeshes;
  }

private:

  math::uuid _mesh_id;
  std::vector<submesh> _submeshes;

}; // class static_mesh

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_STATIC_MESH_HPP_
