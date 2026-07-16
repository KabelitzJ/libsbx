// SPDX-License-Identifier: MIT
#include <libsbx/graphics/environment_map.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

namespace sbx::graphics {

environment_map::environment_map(const graphics::cube_image2d_handle cube, const graphics::image2d_handle brdf, const graphics::cube_image2d_handle irradiance, const graphics::cube_image2d_handle prefiltered)
: _cube{cube},
  _brdf{brdf},
  _irradiance{irradiance},
  _prefiltered{prefiltered} { }

environment_map::~environment_map() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (_cube.is_valid()) {
    graphics_module.remove_resource(_cube);
  }

  if (_brdf.is_valid()) {
    graphics_module.remove_resource(_brdf);
  }

  if (_irradiance.is_valid()) {
    graphics_module.remove_resource(_irradiance);
  }

  if (_prefiltered.is_valid()) {
    graphics_module.remove_resource(_prefiltered);
  }
}

auto environment_map::type() const -> assets::asset_type {
  return assets::asset_type::environment_map;
}

auto environment_map::cube() const noexcept -> cube_image2d_handle {
  return _cube;
}

auto environment_map::brdf() const noexcept -> image2d_handle {
  return _brdf;
}

auto environment_map::irradiance() const noexcept -> cube_image2d_handle {
  return _irradiance;
}

auto environment_map::prefiltered() const noexcept -> cube_image2d_handle {
  return _prefiltered;
}

} // namespace sbx::graphics