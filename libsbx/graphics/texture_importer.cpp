// SPDX-License-Identifier: MIT
#include <libsbx/graphics/assets/texture_importer.hpp>

#include <libsbx/assets/importer_registry.hpp>

namespace sbx::graphics {

auto texture_importer::type() const -> std::string_view {
  return texture::type_name;
}

auto texture_importer::import(const assets::import_context& context) -> std::unique_ptr<assets::asset_base> {
  auto settings = context.settings;

  const auto use_srgb = settings["srgb"].as<bool>(true);
  const auto use_mipmap = settings["mipmap"].as<bool>(false);
  const auto use_anisotropic = settings["anisotropic"].as<bool>(false);

  const auto format = use_srgb ? graphics::format::r8g8b8a8_srgb : graphics::format::r8g8b8a8_unorm;
  const auto filter = _parse_filter(settings["filter"].as<std::string>("linear"));
  const auto address_mode = _parse_address_mode(settings["address_mode"].as<std::string>("repeat"));

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto handle = graphics_module.add_resource<graphics::image2d>(context.resolved, format, filter, address_mode, use_anisotropic, use_mipmap);

  return std::make_unique<texture>(handle);
}

auto texture_importer::_parse_filter(const std::string& value) -> graphics::filter {
  if (value == "nearest") {
    return graphics::filter::nearest;
  }

  return graphics::filter::linear;
}

auto texture_importer::_parse_address_mode(const std::string& value) -> graphics::address_mode {
  if (value == "clamp" || value == "clamp_to_edge") {
    return graphics::address_mode::clamp_to_edge;
  }

  return graphics::address_mode::repeat;
}

const auto texture_importer_registered = sbx::assets::register_importer<sbx::graphics::texture_importer>({".png", ".jpg", ".jpeg", ".tga", ".sbxtex"});

} // namespace sbx::graphics
