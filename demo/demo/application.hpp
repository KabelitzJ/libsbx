// SPDX-License-Identifier: MIT
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

  auto is_paused() const -> bool override;

private:

  auto _randomize_terrain() -> void;
  auto _smooth_terrain() -> void;
  auto _rebuild_terrain_tiles() -> void;

  auto _generate_brdf(const std::uint32_t size) -> void;
  auto _generate_irradiance(const std::uint32_t size) -> void;
  auto _generate_prefiltered(const std::uint32_t size) -> void;

  bool _is_paused;

  sbx::math::angle _rotation;

  sbx::scenes::node _rune0_emitter;
  sbx::scenes::node _rune1_emitter;
  sbx::scenes::node _rune2_emitter;
  sbx::scenes::node _rune3_emitter;

  sbx::graphics::image2d_handle _brdf;
  sbx::graphics::cube_image2d_handle _irradiance;
  sbx::graphics::cube_image2d_handle _prefiltered;

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
