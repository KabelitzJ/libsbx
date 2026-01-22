// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_MESH_HPP_
#define LIBSBX_UI_MESH_HPP_

#include <libsbx/graphics/pipeline/mesh.hpp>

#include <libsbx/ui/vertex2d.hpp>

namespace sbx::ui {

class mesh : public graphics::mesh<vertex2d> {

public:

  mesh(std::vector<vertex2d>&& vertices, std::vector<std::uint32_t>&& indices)
  : graphics::mesh<vertex2d>{std::move(vertices), std::move(indices)} { }

  ~mesh() override = default;

}; // class mesh

} // namespace sbx::ui

#endif // LIBSBX_UI_MESH_HPP_
