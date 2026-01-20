#ifndef DEMO_APPLICATION_HPP_
#define DEMO_APPLICATION_HPP_

#include <libsbx/units/units.hpp>
#include <libsbx/utility/utility.hpp>
#include <libsbx/math/math.hpp>
#include <libsbx/core/core.hpp>
#include <libsbx/devices/devices.hpp>
#include <libsbx/graphics/graphics.hpp>
#include <libsbx/models/models.hpp>
#include <libsbx/scenes/scenes.hpp>

#include <demo/dual_grid.hpp>

namespace demo {

struct grid_cell_data {
  bool is_painted = false;
  std::uint8_t last_mask = 0u;

  sbx::scenes::node node{sbx::scenes::node::null};

  // Optional: keep stable visuals per cell
  std::float_t height = 3.0f;
  sbx::math::color color = sbx::math::color::white();
}; // struct grid_cell_data

struct terrain_tag {
  sbx::math::color color;
  sbx::math::uuid mesh_id;
  std::uint32_t grid_cell;
  std::float_t height;
}; // struct terrain_tag

class application : public sbx::core::application {

public:

  using grid_type = dual_grid<grid_cell_data>;

  application();

  ~application() override = default;

  auto update() -> void override;

  auto fixed_update() -> void override;

  auto grid() const -> const grid_type& {
    return _grid;
  }

private:

  auto _rebuild_terrain_tiles() -> void;

  auto _generate_brdf(const std::uint32_t size) -> void;
  auto _generate_irradiance(const std::uint32_t size) -> void;
  auto _generate_prefiltered(const std::uint32_t size) -> void;

  // auto _generate_icosphere(const std::float_t radius, const std::uint32_t subdivisions) -> std::unique_ptr<sbx::models::mesh>;

  sbx::math::angle _rotation;

  sbx::scenes::node _player;

  sbx::graphics::storage_buffer_handle _selection_buffer;

  // std::vector<tank> _tanks;

  sbx::scenes::node _light_center;
  sbx::scenes::node _fox1;
  sbx::scenes::node _fox2;
  sbx::scenes::node _cube2;
  sbx::scenes::node _duck;
  sbx::scenes::node _helmet;

  sbx::graphics::image2d_handle _brdf;
  sbx::graphics::cube_image2d_handle _irradiance;
  sbx::graphics::cube_image2d_handle _prefiltered;

  grid_type _grid;

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
