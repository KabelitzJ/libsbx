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
#include <libsbx/ui/ui.hpp>
#include <libsbx/scenes/scenes.hpp>

#include <demo/building/building_definition.hpp>
#include <demo/building/road_types.hpp>

namespace demo {

class application : public sbx::core::application {

public:

  application();

  ~application() override = default;

  auto update() -> void override;

  auto fixed_update() -> void override;

  auto is_paused() const -> bool override;

private:

  auto _build_ui() -> void;

  auto _register_buildings() -> void;
  auto _update_placement() -> void;
  auto _raycast_terrain() -> std::optional<sbx::math::vector3>;

  auto _update_road_drawing() -> void;

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

  sbx::ui::font _font;

  // Placement state
  bool _placement_active{false};
  std::uint32_t _placement_definition_id{0};
  orientation _placement_orientation{orientation::n};

  // Sculpt state
  bool _sculpt_raise{true};

  // Road drawing state
  bool _is_drawing_road{false};
  chunk_coord _road_draw_previous_cell{};
  road_type _current_road_type{road_type::paved};

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
