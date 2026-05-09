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

  bool _debug_frustum_active{false};
  sbx::math::vector3 _debug_frustum_position{0.0f, 5.0f, 0.0f};
  std::float_t _debug_frustum_yaw{0.0f};
  std::float_t _debug_frustum_pitch{0.0f};

  sbx::units::second _time_accumulator{0};
  std::uint32_t _frame_count{0u};

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
