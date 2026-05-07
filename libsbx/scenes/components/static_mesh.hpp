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

  static_mesh() = default;

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

  auto set_mesh_id(const math::uuid mesh_id) noexcept -> void {
    _mesh_id = mesh_id;
  }

  auto submeshes() const noexcept -> const std::vector<submesh>& {
    return _submeshes;
  }

  auto submeshes() noexcept -> std::vector<submesh>& {
    return _submeshes;
  }

  auto resize_submeshes(const std::size_t count) -> void {
    const auto previous = _submeshes.size();

    _submeshes.resize(count);

    for (auto i = previous; i < count; ++i) {
      _submeshes[i] = submesh{static_cast<std::uint32_t>(i), math::uuid::nil()};
    }
  }

  auto set_submesh_material(const std::size_t index, const math::uuid material) -> void {
    if (index >= _submeshes.size()) {
      return;
    }

    _submeshes[index].material = material;
  }

private:

  math::uuid _mesh_id;
  std::vector<submesh> _submeshes;

}; // class static_mesh

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_COMPONENTS_STATIC_MESH_HPP_
