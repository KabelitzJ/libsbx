// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_TERRAIN_MODULE_HPP_
#define DEMO_TERRAIN_TERRAIN_MODULE_HPP_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include <libsbx/core/module.hpp>
#include <libsbx/graphics/graphics.hpp>

#include <demo/terrain/heightmap.hpp>
#include <demo/terrain/province_map.hpp>
#include <demo/terrain/splat_generator.hpp>
#include <demo/terrain/terrain_types.hpp>

namespace demo {

struct terrain_config {
  std::filesystem::path heightmap_path;
  std::filesystem::path province_ids_path;
  std::filesystem::path borders_path;
  std::filesystem::path metadata_path;
  std::float_t cell_size{0.05f};
  std::float_t height_scale{30.0f};
  splat_generation_params splat{};
}; // struct terrain_config

class terrain_module final : public sbx::core::module<terrain_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<sbx::graphics::graphics_module>{});

  inline static const auto config = terrain_config{
    .heightmap_path = "res://map/heightmap.r16",
    .province_ids_path = "res://map/province_ids.r32u",
    .borders_path = "res://map/borders.f32",
    .metadata_path = "res://map/provinces.yaml",
    .cell_size = 0.5f,
    .height_scale = 5.0f,
  };

public:

  terrain_module();
  ~terrain_module() override = default;

  auto update() -> void override;

  auto load() -> void;

  auto is_loaded() const -> bool;

  auto height_buffer() const -> sbx::graphics::storage_buffer_handle;
  auto splat_buffer() const -> sbx::graphics::storage_buffer_handle;
  auto province_buffer() const -> sbx::graphics::storage_buffer_handle;
  auto borders_buffer() const -> sbx::graphics::storage_buffer_handle;
  auto edge_count() const -> std::uint32_t;

  auto heightmap() -> demo::heightmap&;
  auto heightmap() const -> const demo::heightmap&;

  auto provinces() -> demo::province_map&;
  auto provinces() const -> const demo::province_map&;

  auto cell_size() const -> std::float_t;
  auto height_scale() const -> std::float_t;

  auto vertices_x() const -> std::uint32_t;
  auto vertices_z() const -> std::uint32_t;
  auto cells_x() const -> std::uint32_t;
  auto cells_z() const -> std::uint32_t;

  auto world_extent_x() const -> std::float_t;
  auto world_extent_z() const -> std::float_t;
  auto offset_x() const -> std::float_t;
  auto offset_z() const -> std::float_t;

  auto get_height_at(const world_coordinates& coordinates) const -> std::float_t;
  auto get_landform_at(const world_coordinates& coordinates) const -> landform;
  auto get_province_at(const world_coordinates& coordinates) const -> province_map::province_id;

  auto splat_data() const -> const splat_weights*;
  auto splat_data_size_bytes() const -> std::size_t;

  auto selected_province_id() const -> province_map::province_id;
  auto set_selected_province_id(province_map::province_id id) -> void;

private:

  struct metadata_info {
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t edge_count;
  }; // struct metadata_info

  static auto _read_metadata(const std::filesystem::path& path) -> metadata_info;

  auto _upload_gpu_data() -> void;

  std::optional<demo::heightmap> _heightmap;
  std::optional<demo::province_map> _province_map;
  std::vector<splat_weights> _splat_weights;
  bool _splat_dirty;
  bool _province_dirty;
  bool _borders_dirty;
  bool _loaded;

  std::uint32_t _edge_count{0};
  std::vector<std::float_t> _edges_world;

  province_map::province_id _selected_province_id{province_map::invalid_id};

  sbx::graphics::storage_buffer_handle _height_buffer;
  sbx::graphics::storage_buffer_handle _splat_buffer;
  sbx::graphics::storage_buffer_handle _province_buffer;
  sbx::graphics::storage_buffer_handle _borders_buffer;

}; // class terrain_module

} // namespace demo

#endif // DEMO_TERRAIN_TERRAIN_MODULE_HPP_
