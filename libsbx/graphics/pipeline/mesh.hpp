// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_PIPELINE_MESH_HPP_
#define LIBSBX_GRAPHICS_PIPELINE_MESH_HPP_

#include <memory>
#include <vector>
#include <string>
#include <array>
#include <unordered_map>

#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/assert.hpp>

#include <libsbx/math/volume.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/graphics/buffers/buffer.hpp>
#include <libsbx/graphics/buffers/storage_buffer.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/pipeline/vertex_input_description.hpp>

#include <libsbx/graphics/resource_storage.hpp>

namespace sbx::graphics {

struct submesh {
  std::uint32_t index_count;
  std::uint32_t index_offset;
  std::uint32_t vertex_count;
  std::uint32_t vertex_offset;
  math::volume bounds;
  math::matrix4x4 local_transform;
  utility::hashed_string name;
  std::uint32_t material_index;
  std::uint32_t lod_level{0u};
  std::uint32_t lod_group{0u};
}; // struct submesh

template<vertex Vertex>
class mesh {

public:

  using vertex_type = Vertex;

  using index_type = std::uint32_t;

  struct mesh_data {
    std::vector<vertex_type> vertices;
    std::vector<index_type> indices;
    std::vector<graphics::submesh> submeshes;
    math::volume bounds;
    std::array<std::vector<math::vector4>, 5u> streams{};
  }; // struct mesh_data

  mesh(const std::vector<vertex_type>& vertices, const std::vector<index_type>& indices, const math::volume& bounds = math::volume{});

  mesh(std::vector<vertex_type>&& vertices, std::vector<index_type>&& indices, const math::volume& bounds = math::volume{});

  mesh(const mesh& other) noexcept = delete;

  virtual ~mesh();

  auto render(graphics::command_buffer& command_buffer, std::uint32_t instance_count = 1u) const -> void;

  auto render_submesh(graphics::command_buffer& command_buffer, std::uint32_t submesh_index, std::uint32_t instance_count = 1u) const -> void;

  auto address() const -> std::uint64_t;

  auto bind(graphics::command_buffer& command_buffer) const -> void;

  auto render_submesh_indirect(graphics::storage_buffer& buffer, std::uint32_t offset, std::uint32_t submesh_index, std::uint32_t instance_count = 1u) const -> void;

  auto submeshes() const noexcept -> const std::vector<graphics::submesh>&;

  auto submesh_index(const utility::hashed_string& name) const -> std::uint32_t {
    const auto entry = std::ranges::find(_submeshes, name, &graphics::submesh::name);

    if  (entry == _submeshes.end()) {
      throw utility::runtime_error{"Submesh '{}' not found", name.str()};
    }
  
    return std::distance(_submeshes.begin(), entry);
  }

  auto submesh(std::uint32_t submesh_index) const -> const graphics::submesh& {
    utility::assert_that(submesh_index < _submeshes.size(), fmt::format("Trying to access out of bounds submesh {} of mesh with {} submeshes", submesh_index, _submeshes.size()));
    return _submeshes.at(submesh_index);
  }

  auto submesh(const utility::hashed_string& name) const -> const graphics::submesh& {
    return submesh(submesh_index(name));
  }

  auto submesh_bounds(std::uint32_t submesh_index) const -> const math::volume& {
    return submesh(submesh_index).bounds;
  }

  auto submesh_bounds(const utility::hashed_string& name) const -> const math::volume& {
    return submesh(submesh_index(name)).bounds;
  }

  auto submesh_local_transform(std::uint32_t submesh_index) const -> const math::matrix4x4& {
    return submesh(submesh_index).local_transform;
  }

  auto submesh_local_transform(const utility::hashed_string& name) const -> const math::matrix4x4& {
    return submesh(submesh_index(name)).local_transform;
  }

  auto submesh_names() const -> std::unordered_map<utility::hashed_string, std::uint32_t> {
    auto names = std::unordered_map<utility::hashed_string, std::uint32_t>{};

    for (std::size_t i = 0; i < _submeshes.size(); ++i) {
      names.emplace(_submeshes[i].name, static_cast<std::uint32_t>(i));
    }

    return names;
  }

  auto bounds() const -> const math::volume& {
    return _bounds;
  }

  auto vertex_count() const noexcept -> std::uint32_t {
    return _vertex_count;
  }

  auto index_count() const noexcept -> std::uint32_t {
    return _index_count;
  }

  auto lod_count(std::uint32_t submesh_index) const -> std::uint32_t {
    const auto group = _submeshes[submesh_index].lod_group;
    auto count = std::uint32_t{0u};

    for (const auto& submesh : _submeshes) {
      if (submesh.lod_group == group) {
        ++count;
      }
    }

    return count;
  }

  auto find_base_submesh_index(std::uint32_t lod_group) const -> std::optional<std::uint32_t> {
    for (auto i = std::uint32_t{0u}; i < static_cast<std::uint32_t>(_submeshes.size()); ++i) {
      if (_submeshes[i].lod_group == lod_group && _submeshes[i].lod_level == 0u) {
        return i;
      }
    }

    return std::nullopt;
  }

protected:

  mesh(mesh_data&& mesh_data);

  auto _upload_vertices(const std::vector<vertex_type>& vertices, const std::vector<index_type>& indices) -> void;

  auto _upload_vertices(std::vector<vertex_type>&& vertices, std::vector<index_type>&& indices) -> void;

  auto _calculate_bounds_from_submeshes(math::volume&& bounds) const -> math::volume;

  buffer_handle _vertex_buffer;
  buffer_handle _index_buffer;

  std::vector<graphics::submesh> _submeshes;
  std::uint32_t _vertex_count;
  std::uint32_t _index_count;

  math::volume _bounds;

}; // class mesh

} // namespace sbx::graphics

#include <libsbx/graphics/pipeline/mesh.ipp>

#endif // LIBSBX_GRAPHICS_PIPELINE_MESH_HPP_
