// SPDX-License-Identifier: MIT
#include <demo/terrain_subrenderer.hpp>

#include <libsbx/scenes/scenes.hpp>

#include <libsbx/graphics/graphics.hpp>

namespace demo {

terrain_subrenderer::terrain_subrenderer(
  const std::vector<sbx::graphics::attachment_description>& attachments,
  const std::filesystem::path& path,
  const terrain& terrain
)
: sbx::graphics::subrenderer{},
  _pipeline{path, attachments},
  _push_handler{_pipeline},
  _descriptor_handler{_pipeline, 0u},
  _verts_w{terrain.verts_w()},
  _verts_h{terrain.verts_h()} {

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto height_buffer_size = static_cast<VkDeviceSize>(_verts_w * _verts_h * sizeof(std::float_t));

  _height_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(
    std::max(height_buffer_size, static_cast<VkDeviceSize>(sbx::graphics::storage_buffer::min_size)),
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  );

  auto indices = _generate_indices();
  _index_count = static_cast<std::uint32_t>(indices.size());

  auto index_buffer_size = static_cast<VkDeviceSize>(indices.size() * sizeof(std::uint32_t));

  _index_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(
    std::max(index_buffer_size, static_cast<VkDeviceSize>(sbx::graphics::storage_buffer::min_size)),
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
  );

  auto& height_buf = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);

  auto initial_heights = std::vector<std::float_t>(_verts_w * _verts_h, 0.0f);

  _update_buffer(height_buf, initial_heights);

  auto& index_buf = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);

  _update_buffer(index_buf, indices);
}

auto terrain_subrenderer::_generate_indices() -> std::vector<std::uint32_t> {
  auto indices = std::vector<std::uint32_t>{};
  indices.reserve((_verts_w - 1) * (_verts_h - 1) * 6);

  for (auto y = 0u; y < _verts_h - 1; ++y) {
    for (auto x = 0u; x < _verts_w - 1; ++x) {
      auto tl = y * _verts_w + x;
      auto tr = tl + 1;
      auto bl = (y + 1) * _verts_w + x;
      auto br = bl + 1;

      indices.push_back(tl);
      indices.push_back(bl);
      indices.push_back(tr);

      indices.push_back(tr);
      indices.push_back(bl);
      indices.push_back(br);
    }
  }

  return indices;
}

auto terrain_subrenderer::render(
  sbx::graphics::command_buffer& command_buffer
) -> void {
  SBX_PROFILE_SCOPE("terrain_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scenes_module.scene();

  auto& height_buf = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);

  auto& index_buf = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", scene.uniform_handler());

  auto offset_x = static_cast<float>(_verts_w - 1) * grid::cell_size * 0.5f;
  auto offset_z = static_cast<float>(_verts_h - 1) * grid::cell_size * 0.5f;

  _push_handler.push("height_data_buffer", height_buf.address());
  _push_handler.push("verts_w", _verts_w);
  _push_handler.push("verts_h", _verts_h);
  _push_handler.push("inv_verts_w", 1.0f / static_cast<float>(_verts_w));
  _push_handler.push("cell_size", grid::cell_size);
  _push_handler.push("offset_x", offset_x);
  _push_handler.push("offset_z", offset_z);

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);
  _push_handler.bind(command_buffer);

  command_buffer.bind_index_buffer(index_buf.handle(), 0, VK_INDEX_TYPE_UINT32);

  command_buffer.draw_indexed(_index_count, 1, 0, 0, 0);
}

auto terrain_subrenderer::update_heights(
  const terrain& terrain
) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& height_buf = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);

  auto heights = std::vector<std::float_t>{};
  heights.reserve(_verts_w * _verts_h);

  for (auto y = 0u; y < _verts_h; ++y) {
    for (auto x = 0u; x < _verts_w; ++x) {
      heights.push_back(terrain.get_height(x, y));
    }
  }

  _update_buffer(height_buf, heights);
}

auto terrain_subrenderer::update_heights_region(
  const terrain& terrain,
  std::uint32_t min_x, std::uint32_t min_y,
  std::uint32_t max_x, std::uint32_t max_y
) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& height_buf = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);

  // Expand by 1 for correct normals at edges
  auto safe_min_x = (min_x > 0u) ? min_x - 1u : 0u;
  auto safe_min_y = (min_y > 0u) ? min_y - 1u : 0u;
  auto safe_max_x = std::min(max_x + 1u, _verts_w - 1u);
  auto safe_max_y = std::min(max_y + 1u, _verts_h - 1u);

  // Full region update (simple approach for now)
  auto heights = std::vector<std::float_t>{};
  heights.reserve(_verts_w * _verts_h);

  for (auto y = 0u; y < _verts_h; ++y) {
    for (auto x = 0u; x < _verts_w; ++x) {
      heights.push_back(terrain.get_height(x, y));
    }
  }

  _update_buffer(height_buf, heights);
}

} // namespace demo
