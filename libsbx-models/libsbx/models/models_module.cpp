// models_module.cpp
#include <libsbx/models/models_module.hpp>

#include <filesystem>
#include <string>
 
#include <yaml-cpp/yaml.h>

#include <libsbx/utility/hashed_string.hpp>
#include <libsbx/utility/logger.hpp>
 
#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/images/image2d.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene_asset_table.hpp>
#include <libsbx/scenes/asset_io.hpp>
#include <libsbx/scenes/scene_asset_table.hpp>

#include <libsbx/models/mesh.hpp>
#include <libsbx/models/material.hpp>

#include <libsbx/models/material.hpp>

namespace sbx::models {

auto _parse_format(const std::string& str) -> graphics::format {
  if (str == "r8g8b8a8_srgb") {
    return graphics::format::r8g8b8a8_srgb;
  }
 
  if (str == "r8g8b8a8_unorm") {
    return graphics::format::r8g8b8a8_unorm;
  }
 
  return graphics::format::r8g8b8a8_unorm;
}
 
auto _parse_alpha_mode(const std::string& str) -> alpha_mode {
  if (str == "mask") {
    return alpha_mode::mask;
  }
 
  if (str == "blend") {
    return alpha_mode::blend;
  }
 
  return alpha_mode::opaque;
}
 
auto _resolve_texture(scenes::scene_asset_table& assets, const YAML::Node& node) -> graphics::image2d_handle {
  if (!node || !node["image"]) {
    return graphics::image2d_handle{};
  }
 
  const auto image_path = node["image"].as<std::string>();
  const auto format_str = node["format"].as<std::string>("r8g8b8a8_unorm");
  const auto format = _parse_format(format_str);
 
  const auto key = utility::hashed_string{image_path};
 
  if (!assets.has_image(key)) {
    assets.add_image(key, image_path, format);
  }
 
  return assets.get_image(key);
}

inline auto _load_material_yaml(scenes::scene_asset_table& assets, const utility::hashed_string& name, const std::filesystem::path& path) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
 
  const auto resolved_path = assets_module.resolve_path(path);
 
  if (!std::filesystem::exists(resolved_path)) {
    utility::logger<"models">::warn("Material file not found: '{}'", resolved_path.string());
    assets.add_material<material>(name);
    return;
  }
 
  const auto node = YAML::LoadFile(resolved_path.string());
 
  auto material = models::material{};
 
  // Textures
  material.albedo.image = _resolve_texture(assets, node["albedo"]);
  material.normal.image = _resolve_texture(assets, node["normal"]);
  material.metallic_roughness.image = _resolve_texture(assets, node["metallic_roughness"]);
  material.occlusion.image = _resolve_texture(assets, node["occlusion"]);
  material.emissive.image = _resolve_texture(assets, node["emissive"]);
 
  // Emissive factor
  if (node["emissive"] && node["emissive"]["factor"]) {
    const auto& f = node["emissive"]["factor"];
 
    material.emissive_factor = math::vector4{f[0].as<std::float_t>(0.0f), f[1].as<std::float_t>(0.0f), f[2].as<std::float_t>(0.0f), 1.0f};
  }
 
  // Scalar properties
  if (node["base_color"]) {
    const auto& c = node["base_color"];
 
    material.base_color = math::color{c[0].as<std::float_t>(1.0f), c[1].as<std::float_t>(1.0f), c[2].as<std::float_t>(1.0f), c[3].as<std::float_t>(1.0f)};
  }
 
  material.metallic_factor = node["metallic_factor"].as<std::float_t>(1.0f);
  material.roughness_factor = node["roughness_factor"].as<std::float_t>(1.0f);
  material.specular_factor = node["specular_factor"].as<std::float_t>(1.0f);
 
  // Alpha
  material.alpha = _parse_alpha_mode(node["alpha_mode"].as<std::string>("opaque"));
  material.alpha_cutoff = node["alpha_cutoff"].as<std::float_t>(0.5f);
 
  // Double sided
  material.is_double_sided = node["double_sided"].as<bool>(false);
 
  // Vegetation sway
  if (node["sway"]) {
    const auto& sway = node["sway"];
 
    material.sway_speed = sway["speed"].as<std::float_t>(0.0f);
    material.sway_strength = sway["strength"].as<std::float_t>(0.0f);
    material.sway_falloff_exponent = sway["falloff_exponent"].as<std::float_t>(1.0f);
  }
 
  if (node["scrumble"]) {
    const auto& scrumble = node["scrumble"];
 
    material.scrumble_speed = scrumble["speed"].as<std::float_t>(0.0f);
    material.scrumble_strength = scrumble["strength"].as<std::float_t>(0.0f);
    material.scrumble_falloff_exponent = scrumble["falloff_exponent"].as<std::float_t>(1.0f);
  }
 
  assets.add_material<models::material>(name, std::move(material));
}

models_module::models_module() {
  auto& scenes_module = core::engine::get_module<sbx::scenes::scenes_module>();

  auto& asset_io = scenes_module.get_asset_io_registry();

  asset_io.register_loader("static_meshes", [](sbx::scenes::scene_asset_table& assets, const sbx::utility::hashed_string& name, const YAML::Node& node) -> void {
    const auto path = node["path"].as<std::string>();

    assets.add_mesh<mesh>(name, path);
  });

  asset_io.register_loader("materials", [](sbx::scenes::scene_asset_table& assets, const sbx::utility::hashed_string& name, const YAML::Node& node) -> void {
    const auto path = node["path"].as<std::string>("");
 
    if (path.empty()) {
      assets.add_material<material>(name);
      return;
    }
 
    _load_material_yaml(assets, name, path);
  });
}

models_module::~models_module() {

}

auto models_module::update() -> void {

}

} // namespace sbx::models
