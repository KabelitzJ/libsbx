// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_ENVIRONMENT_MAP_IMPORTER_HPP_
#define LIBSBX_GRAPHICS_ENVIRONMENT_MAP_IMPORTER_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/asset.hpp>
#include <libsbx/assets/importer_registry.hpp>
#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/images/cube_image.hpp>

#include <libsbx/graphics/environment_map.hpp>

namespace sbx::graphics {

/**
 * @brief Imports a `.envmap.yaml` descriptor into an environment_map.
 *
 * Descriptor fields:
 *   cube (required URI of the cube map source), suffix (face suffix, default ".png"),
 *   irradiance_size (default 64), prefiltered_size (default 512), brdf_size (default 512).
 *
 * Loads the cube and bakes the BRDF / irradiance / prefiltered set at load time.
 */
class environment_map_importer final : public assets::importer<environment_map_importer> {

  inline static const auto is_registered = register_importer({".envmap.yaml"});

public:

  auto type() const -> std::string_view override;

  auto import(const assets::import_context& context) -> std::unique_ptr<assets::asset_base> override;

private:

  static auto _generate_brdf(const std::uint32_t size) -> graphics::image2d_handle;
  static auto _generate_irradiance(const cube_image2d_handle source, const std::uint32_t size) -> graphics::cube_image2d_handle;
  static auto _generate_prefiltered(const cube_image2d_handle source, const std::uint32_t size) -> graphics::cube_image2d_handle;

}; // class environment_map_importer

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_ENVIRONMENT_MAP_IMPORTER_HPP_
