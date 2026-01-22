// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_MESH_HPP_
#define LIBSBX_MODELS_MESH_HPP_

#include <filesystem>

#include <libsbx/utility/hash.hpp>

#include <libsbx/math/volume.hpp>
#include <libsbx/math/sphere.hpp>

#include <libsbx/io/loader_factory.hpp>

#include <libsbx/graphics/pipeline/mesh.hpp>

#include <libsbx/models/vertex3d.hpp>

namespace sbx::models {

class mesh : public graphics::mesh<vertex3d>, public io::loader_factory<mesh, graphics::mesh<vertex3d>::mesh_data> {

  using base = graphics::mesh<vertex3d>;

public:

  struct file_header {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t index_type_size;
    std::uint32_t index_count;
    std::uint32_t vertex_type_size;
    std::uint32_t vertex_count;
    std::uint32_t submesh_count;
  }; // struct file_header

  using mesh_data = graphics::mesh<vertex3d>::mesh_data;

  using base::mesh;

  mesh(const std::filesystem::path& path);

  ~mesh() override;

private:

  static auto _load(const std::filesystem::path& path) -> mesh_data;

  static auto _process(const std::filesystem::path& path, const mesh_data& data) -> void;

}; // class mesh

} // namespace sbx::models

#endif // LIBSBX_MODELS_MESH_HPP_
