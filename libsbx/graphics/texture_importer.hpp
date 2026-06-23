// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_ASSETS_TEXTURE_IMPORTER_HPP_
#define LIBSBX_GRAPHICS_ASSETS_TEXTURE_IMPORTER_HPP_

#include <memory>
#include <string>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/asset.hpp>
#include <libsbx/assets/importer_registry.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/images/image2d.hpp>

#include <libsbx/graphics/texture.hpp>

namespace sbx::graphics {

/**
 * @brief Imports an image file (or cooked .sbxtex) into a texture asset.
 *
 * Reads optional knobs from the asset's .meta `import_settings`:
 *   srgb (bool, default true), mipmap (bool, default false), anisotropic (bool, default false),
 *   filter ("linear" | "nearest", default "linear"), address_mode ("repeat" | "clamp", default "repeat").
 */
class texture_importer final : public assets::importer<texture_importer> {

  inline static const auto is_registered = register_importer({".png", ".jpg", ".jpeg", ".tga", ".sbxtex"});

public:

  texture_importer() = default;

  ~texture_importer() override = default;

  auto type() const -> std::string_view override;

  auto import(const assets::import_context& context) -> std::unique_ptr<assets::asset_base> override;

private:

  static auto _parse_filter(const std::string& value) -> graphics::filter;

  static auto _parse_address_mode(const std::string& value) -> graphics::address_mode;

}; // class texture_importer

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_ASSETS_TEXTURE_IMPORTER_HPP_
