// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_ENVIRONMENT_MAP_HPP_
#define LIBSBX_GRAPHICS_ENVIRONMENT_MAP_HPP_

#include <string_view>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/asset.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::graphics {

class environment_map final : public assets::asset_base {

public:

  inline static constexpr auto type_name = std::string_view{"environment_map"};

  explicit environment_map() {

  }

  ~environment_map() override {

  }

  auto type() const -> assets::asset_type override {
    return assets::asset_type::environment_map;
  }

private:

  auto _generate_brdf(const std::uint32_t size) -> void;
  auto _generate_irradiance(const std::uint32_t size) -> void;
  auto _generate_prefiltered(const std::uint32_t size) -> void;

  graphics::cube_image2d_handle _skybox;
  graphics::image2d_handle _brdf;
  graphics::cube_image2d_handle _irradiance;
  graphics::cube_image2d_handle _prefiltered;

}; // class environment_map

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_ENVIRONMENT_MAP_HPP_