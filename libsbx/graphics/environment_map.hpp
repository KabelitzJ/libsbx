// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_ENVIRONMENT_MAP_HPP_
#define LIBSBX_GRAPHICS_ENVIRONMENT_MAP_HPP_

#include <string_view>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/asset.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/images/image2d.hpp>

namespace sbx::graphics {

class environment_map final : public assets::asset {

public:

  inline static constexpr auto type_name = std::string_view{"environment_map"};

  explicit environment_map(const graphics::cube_image2d_handle cube, const graphics::image2d_handle brdf, const graphics::cube_image2d_handle irradiance, const graphics::cube_image2d_handle prefiltered);

  environment_map(const environment_map& other) = delete;

  environment_map(environment_map&& other)
  : _cube{other._cube},
    _brdf{other._brdf},
    _irradiance{other._irradiance},
    _prefiltered{other._prefiltered} {
    other._cube = graphics::cube_image2d_handle{};
    other._brdf = graphics::image2d_handle{};
    other._irradiance = graphics::cube_image2d_handle{};
    other._prefiltered = graphics::cube_image2d_handle{};
  }

  auto operator=(const environment_map& other) -> environment_map& = delete;

  auto operator=(environment_map&& other) -> environment_map& {
    if (this != &other) {
      _cube = other._cube;
      _brdf = other._brdf;
      _irradiance = other._irradiance;
      _prefiltered = other._prefiltered;

      other._cube = cube_image2d_handle{};
      other._brdf = image2d_handle{};
      other._irradiance = cube_image2d_handle{};
      other._prefiltered = cube_image2d_handle{};
    }

    return *this;
  }

  ~environment_map() override ;

  auto type() const -> assets::asset_type override;

  auto cube() const noexcept -> cube_image2d_handle;
 
  auto brdf() const noexcept -> image2d_handle;
 
  auto irradiance() const noexcept -> cube_image2d_handle;
 
  auto prefiltered() const noexcept -> cube_image2d_handle;

private:

  graphics::cube_image2d_handle _cube;
  graphics::image2d_handle _brdf;
  graphics::cube_image2d_handle _irradiance;
  graphics::cube_image2d_handle _prefiltered;

}; // class environment_map

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_ENVIRONMENT_MAP_HPP_
