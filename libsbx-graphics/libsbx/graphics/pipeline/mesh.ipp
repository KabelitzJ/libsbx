// SPDX-License-Identifier: MIT
#include <libsbx/graphics/pipeline/mesh.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

template<vertex Vertex>
mesh<Vertex>::mesh(const std::vector<vertex_type>& vertices, const std::vector<index_type>& indices, const math::volume& bounds)
: _vertex_count{static_cast<std::uint32_t>(vertices.size())},
  _index_count{static_cast<std::uint32_t>(indices.size())},
  _bounds{bounds} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _vertex_buffer = graphics_module.add_resource<buffer>(
    (vertices.size() * sizeof(vertex_type)),
    (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), 
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  );

  _index_buffer = graphics_module.add_resource<buffer>(
    (indices.size() * sizeof(index_type)),
    (VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  );

  _submeshes.push_back(graphics::submesh{_index_count, 0, _vertex_count, 0, bounds, math::matrix4x4::identity, utility::hashed_string{"mesh"}});

  _upload_vertices(vertices, indices);
}

template<vertex Vertex>
mesh<Vertex>::mesh(std::vector<vertex_type>&& vertices, std::vector<index_type>&& indices, const math::volume& bounds)
: _vertex_count{static_cast<std::uint32_t>(vertices.size())},
  _index_count{static_cast<std::uint32_t>(indices.size())},
  _bounds{bounds} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _vertex_buffer = graphics_module.add_resource<buffer>(
    (vertices.size() * sizeof(vertex_type)),
    (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), 
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  );

  _index_buffer = graphics_module.add_resource<buffer>(
    (indices.size() * sizeof(index_type)),
    (VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  );

  _submeshes.push_back(graphics::submesh{_index_count, 0, _vertex_count, 0, bounds, math::matrix4x4::identity, utility::hashed_string{"mesh"}});
  
  _upload_vertices(std::move(vertices), std::move(indices));
}

template<vertex Vertex>
mesh<Vertex>::mesh(mesh_data&& mesh_data)
: _submeshes{std::move(mesh_data.submeshes)},
  _vertex_count{static_cast<std::uint32_t>(mesh_data.vertices.size())},
  _index_count{static_cast<std::uint32_t>(mesh_data.indices.size())},
  _bounds{_calculate_bounds_from_submeshes(std::move(mesh_data.bounds))} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _vertex_buffer = graphics_module.add_resource<buffer>(
    (mesh_data.vertices.size() * sizeof(Vertex)),
    (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT), 
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  );

  _index_buffer = graphics_module.add_resource<buffer>(
    (mesh_data.indices.size() * sizeof(index_type)),
    (VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
  );

  _upload_vertices(std::move(mesh_data.vertices), std::move(mesh_data.indices));
}

template<vertex Vertex>
mesh<Vertex>::~mesh() {

}

template<vertex Vertex>
auto mesh<Vertex>::render(graphics::command_buffer& command_buffer, std::uint32_t instance_count) const -> void {
  command_buffer.draw_indexed(_index_count, instance_count, 0, 0, 0);
}

template<vertex Vertex>
auto mesh<Vertex>::render_submesh(graphics::command_buffer& command_buffer, std::uint32_t submesh_index, std::uint32_t instance_count) const -> void {
  const auto& submesh = _submeshes.at(submesh_index);

  command_buffer.draw_indexed(submesh.index_count, instance_count, submesh.index_offset, 0, 0);
}

template<vertex Vertex>
auto mesh<Vertex>::address() const -> std::uint64_t {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& vertex_buffer = graphics_module.get_resource<buffer>(_vertex_buffer); 

  return vertex_buffer.address();
}

template<vertex Vertex>
auto mesh<Vertex>::bind(graphics::command_buffer& command_buffer) const -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& index_buffer = graphics_module.get_resource<buffer>(_index_buffer); 

  // command_buffer.bind_vertex_buffer(0, _vertex_buffer);
  command_buffer.bind_index_buffer(index_buffer, 0, VK_INDEX_TYPE_UINT32);
}

template<vertex Vertex>
auto mesh<Vertex>::render_submesh_indirect(graphics::storage_buffer& buffer, std::uint32_t offset, std::uint32_t submesh_index, std::uint32_t instance_count) const -> void {
  const auto& submesh = _submeshes.at(submesh_index);

  auto command = VkDrawIndexedIndirectCommand{};
  command.indexCount = submesh.index_count,
  command.instanceCount = instance_count,
  command.firstIndex = submesh.index_offset,
  command.vertexOffset = 0u,
  command.firstInstance = 0u,

  buffer.update(&command, sizeof(VkDrawIndexedIndirectCommand), offset * sizeof(VkDrawIndexedIndirectCommand));
}

template<vertex Vertex>
auto mesh<Vertex>::submeshes() const noexcept -> const std::vector<graphics::submesh>& {
  return _submeshes;
}

template<vertex Vertex>
auto mesh<Vertex>::_upload_vertices(const std::vector<vertex_type>& vertices, const std::vector<index_type>& indices) -> void {

}

template<vertex Vertex>
auto mesh<Vertex>::_upload_vertices(std::vector<vertex_type>&& vertices, std::vector<index_type>&& indices) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto vertex_buffer_size = vertices.size() * sizeof(vertex_type);
  auto index_buffer_size = indices.size() * sizeof(index_type);

  auto staging_buffer_size = vertex_buffer_size + index_buffer_size;

  // [NOTE] KAJ 2024-01-19 : We basically store two different types in here. The staging_buffer_size is in bytes so we use std::uint8_t.
  auto staging_buffer = graphics::staging_buffer{staging_buffer_size};

  staging_buffer.write(vertices.data(), vertex_buffer_size);
  staging_buffer.write(indices.data(), index_buffer_size, vertex_buffer_size);

  // _vertex_buffer = std::make_unique<vertex_buffer_type>(vertices.size());
  // _index_buffer = std::make_unique<index_buffer_type>(indices.size());

  auto command_buffer = graphics::command_buffer{true, VK_QUEUE_TRANSFER_BIT};

  auto& index_buffer = graphics_module.get_resource<buffer>(_index_buffer);
  auto& vertex_buffer = graphics_module.get_resource<buffer>(_vertex_buffer); 

  {
    auto copy_region = VkBufferCopy{};
    copy_region.size = vertex_buffer_size;
    copy_region.dstOffset = 0;
    copy_region.srcOffset = 0;

    command_buffer.copy_buffer(staging_buffer, vertex_buffer, copy_region);
  }

  {
    auto copy_region = VkBufferCopy{};
    copy_region.size = index_buffer_size;
    copy_region.dstOffset = 0;
    copy_region.srcOffset = vertex_buffer_size;

    command_buffer.copy_buffer(staging_buffer, index_buffer, copy_region);
  }

  command_buffer.submit_idle();
}

template<vertex Vertex>
auto mesh<Vertex>::_calculate_bounds_from_submeshes(math::volume&& bounds) const -> math::volume {
  if (bounds.min() != math::vector3::zero && bounds.max() != math::vector3::zero) {
    return bounds;
  }

  auto min = _submeshes[0].bounds.max();
  auto max = _submeshes[0].bounds.max();

  for (auto i = 1u; i < _submeshes.size(); ++i) {
    min = math::vector3::min(min, _submeshes[i].bounds.min());
    max = math::vector3::max(max, _submeshes[i].bounds.max());
  }

  return math::volume{min, max};
}

} // namespace sbx::graphics
