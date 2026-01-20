#include <demo/terrain_subrenderer.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes.hpp>

#include <libsbx/graphics/graphics.hpp>

#include <demo/application.hpp>

namespace demo {

terrain_subrenderer::terrain_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path)
: sbx::graphics::subrenderer{},
  _pipeline{path, attachments},
  _push_handler{_pipeline},
  _descriptor_handler{_pipeline, 0u} {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  _grid_vertex_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _grid_quad_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _transform_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _instance_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
}

auto terrain_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("terrain_subrenderer::render");

  auto& application = sbx::core::engine::get_application<demo::application>();

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.scene();

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto& grid_vertex_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_grid_vertex_buffer);
  auto& grid_quad_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_grid_quad_buffer);
  auto& transform_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_transform_buffer);
  auto& instance_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_instance_buffer);

  auto instances_per_mesh = std::unordered_map<sbx::math::uuid, std::vector<instance_data>>{};
  auto transforms = std::vector<transform_data>{};

  auto terrain_query = scene.query<const terrain_tag>();

  for (auto [node, terrain] : terrain_query.each()) {
    const auto& mesh_id = terrain.mesh_id;

    const auto transform_index = static_cast<std::uint32_t>(transforms.size());

    transforms.push_back(transform_data{sbx::math::matrix4x4::identity, scene.world_normal(node)});
    // transforms.push_back(transform_data{sbx::math::matrix_cast<4, 4>(scene.world_rotation(node)), scene.world_normal(node)});

    auto& instances = instances_per_mesh[mesh_id];

    const auto quad_index = static_cast<std::uint32_t>(terrain.grid_cell);

    instances.push_back(instance_data{transform_index, quad_index, terrain.height, terrain.color.r(), terrain.color.g(), terrain.color.b(), 0u, 0u});
  }

  _update_buffer(transform_buffer, transforms);

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", scene.uniform_handler());

  _push_handler.push("grid_vertex_data_buffer", grid_vertex_buffer.address());
  _push_handler.push("grid_quad_data_buffer", grid_quad_buffer.address());
  _push_handler.push("transform_data_buffer", transform_buffer.address());

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  for (const auto& [mesh_id, instances] : instances_per_mesh) {
    auto& mesh = assets_module.get_asset<sbx::models::mesh>(mesh_id);

    _update_buffer(instance_buffer, instances);

    _push_handler.push("instance_data_buffer", instance_buffer.address());

    mesh.bind(command_buffer);

    _push_handler.push("vertex_buffer", mesh.address());

    _push_handler.bind(command_buffer);

    command_buffer.draw_indexed(mesh.index_count(), static_cast<std::uint32_t>(instances.size()), 0u, 0u, 0u);
  }
}

auto terrain_subrenderer::update_dual_grid_data(const dual_grid<grid_cell_data>& grid) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& grid_vertex_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_grid_vertex_buffer);
  auto& grid_quad_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_grid_quad_buffer);

  auto grid_vertex_buffer_data = std::vector<grid_vertex_data>{};
  grid_vertex_buffer_data.reserve(grid.vertices().size());

  for (const auto& vertex : grid.vertices()) {
    grid_vertex_buffer_data.push_back(grid_vertex_data{vertex.position, 0u});
  }

  _update_buffer(grid_vertex_buffer, grid_vertex_buffer_data);

  auto grid_quad_buffer_data = std::vector<grid_quad_data>{};
  grid_quad_buffer_data.reserve(grid.quads().size());

  for (const auto& quad : grid.quads()) {
    grid_quad_buffer_data.push_back({quad.a, quad.b, quad.c, quad.d});
  }

  _update_buffer(grid_quad_buffer, grid_quad_buffer_data);
}

} // namespace demo