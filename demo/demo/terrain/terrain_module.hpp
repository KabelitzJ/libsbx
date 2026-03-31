// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_TERRAIN_MODULE_HPP_
#define DEMO_TERRAIN_TERRAIN_MODULE_HPP_

#include <vector>
#include <cstdint>
#include <cmath>
#include <utility>

#include <libsbx/core/module.hpp>
#include <libsbx/graphics/graphics.hpp>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/grid.hpp>
#include <demo/terrain/heightmap.hpp>
#include <demo/terrain/sculpt.hpp>
#include <demo/terrain/splat_generator.hpp>

namespace demo {

struct terrain_config {
  std::uint32_t world_width{};
  std::uint32_t world_height{};
  terrain_generation_params generation{};
  splat_generation_params splat{};
}; // struct terrain_config

class terrain_module final : public sbx::core::module<terrain_module> {

  inline static const auto is_registered = register_module(stage::normal, dependencies<sbx::graphics::graphics_module>{});

  inline static const auto config = terrain_config{
    .world_width = 1024,
    .world_height = 1024,
    .generation = {
      .base_scale = 0.0025f,
      .height_scale = 40.0f,
      .warp_scale = 0.002f,
      .warp_strength = 80.0f,
      .continental_bias = 0.75f,
      .octaves = 4u,
      .seed_x = -72345.23522f,
      .seed_z = 7154.2462f,
    },
    .splat = {
      .variety_scale = 0.05f,
      .variety_strength = 0.15f,
      .moisture_scale = 0.02f,
      .seed = 0.67235f,
    },
  };

public:

  terrain_module();

  ~terrain_module() override = default;

  auto update() -> void override;

  auto height_buffer() const -> sbx::graphics::storage_buffer_handle;

  auto splat_buffer() const -> sbx::graphics::storage_buffer_handle;

  auto grid() -> demo::grid&;

  auto grid() const -> const demo::grid&;

  auto heightmap() -> demo::heightmap&;

  auto heightmap() const -> const demo::heightmap&;

  auto splat_data() const -> const splat_weights*;

  auto splat_data_size_bytes() const -> std::size_t;

  auto offset_x() const -> std::float_t;

  auto offset_z() const -> std::float_t;

  auto cell_size() const -> std::float_t;

  auto world_to_cell(const world_coordinates& coordinates) const -> cell_coordinates;

  auto cell_to_world(const cell_coordinates& coordinates) const -> world_coordinates;

  auto get_height_at(const world_coordinates& coordinates) const -> std::float_t;

  auto get_slope_at(const cell_coordinates& coordinates) const -> std::float_t;

  auto sculpt(const world_coordinates& coordinates, std::float_t radius, std::float_t strength) -> sculpt_result;

  auto flatten(const world_coordinates& coordinates, std::float_t radius, std::float_t blend_strength = 1.0f) -> sculpt_result;

  auto smooth(const world_coordinates& coordinates, std::float_t radius, std::float_t blend_strength = 0.5f) -> sculpt_result;

  auto level(const world_coordinates& coordinates, std::float_t radius, std::float_t blend_strength = 1.0f) -> sculpt_result;

  auto flatten_footprint(const cell_coordinates& coordinates, std::uint32_t size_width, std::uint32_t size_height) -> void;

  auto get_height_at_cell(const cell_coordinates& coordinates) const -> std::float_t;

  auto can_build_at(const cell_coordinates& coordinates, std::uint32_t size_width, std::uint32_t size_height, std::float_t max_slope = 3.0f) const -> bool;

  auto world_width() const -> std::uint32_t;

  auto world_height() const -> std::uint32_t;

private:

  auto _upload_gpu_data() -> void;

  auto _update_splat_from_result(const sculpt_result& result) -> void;

  demo::grid _grid;
  demo::heightmap _heightmap;
  std::vector<splat_weights> _splat_weights;
  bool _splat_dirty{true};

  sbx::graphics::storage_buffer_handle _height_buffer;
  sbx::graphics::storage_buffer_handle _splat_buffer;

}; // class terrain_module

} // namespace demo

#endif // DEMO_TERRAIN_TERRAIN_MODULE_HPP_
