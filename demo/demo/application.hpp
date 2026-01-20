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
#include <demo/data.hpp>

namespace demo {

class application : public sbx::core::application {

public:

  using grid_type = dual_grid<grid_data>;

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

  std::uint32_t _selected_main_face = grid_type::invalid_id;

  std::vector<std::uint32_t> _dirty_dual_quads{};
  std::vector<dual_quad_tile_data> _dual_quad_tiles{};

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
