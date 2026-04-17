// SPDX-License-Identifier: MIT
#ifndef EDITOR_APPLICATION_HPP_
#define EDITOR_APPLICATION_HPP_

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

  auto _generate_brdf(const std::uint32_t size) -> void;
  auto _generate_irradiance(const std::uint32_t size) -> void;
  auto _generate_prefiltered(const std::uint32_t size) -> void;

  bool _is_paused;

  sbx::graphics::image2d_handle _brdf;
  sbx::graphics::cube_image2d_handle _irradiance;
  sbx::graphics::cube_image2d_handle _prefiltered;

}; // class application

} // namespace demo

#endif // EDITOR_APPLICATION_HPP_
