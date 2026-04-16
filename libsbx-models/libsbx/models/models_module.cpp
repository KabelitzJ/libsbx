// SPDX-License-Identifier: MIT
#include <libsbx/models/models_module.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/asset_io.hpp>
#include <libsbx/scenes/asset_registry.hpp>

#include <libsbx/models/mesh.hpp>
#include <libsbx/models/material.hpp>

namespace sbx::models {

auto parse_format(const std::string& str) -> graphics::format {
  if (str == "r8g8b8a8_srgb") {
    return graphics::format::r8g8b8a8_srgb;
  }
 
  if (str == "r8g8b8a8_unorm") {
    return graphics::format::r8g8b8a8_unorm;
  }
 
  return graphics::format::r8g8b8a8_unorm;
}
 
auto parse_alpha_mode(const std::string& str) -> alpha_mode {
  if (str == "mask") {
    return alpha_mode::mask;
  }
 
  if (str == "blend") {
    return alpha_mode::blend;
  }
 
  return alpha_mode::opaque;
}
 
auto resolve_texture(scenes::asset_registry& registry, const YAML::Node& node) -> graphics::image2d_handle {
  if (!node || !node["image"]) {
    return graphics::image2d_handle{};
  }
 
  const auto image_path = node["image"].as<std::string>();
  const auto format_str = node["format"].as<std::string>("r8g8b8a8_unorm");
  const auto format = parse_format(format_str);
 
  return registry.request_image(utility::hashed_string{image_path}, image_path, format);
}
 
auto load_material(scenes::asset_registry& registry, const utility::hashed_string& name, const std::filesystem::path& path) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
 
  const auto resolved_path = assets_module.resolve_path(path);
 
  if (!std::filesystem::exists(resolved_path)) {
    utility::logger<"models">::warn("Material file not found: '{}'", resolved_path.string());
    registry.request_material<material>(name);
    return;
  }
 
  const auto node = YAML::LoadFile(resolved_path.string());
 
  auto material = models::material{};
 
  material.albedo.image = resolve_texture(registry, node["albedo"]);
  material.normal.image = resolve_texture(registry, node["normal"]);
  material.metallic_roughness.image = resolve_texture(registry, node["metallic_roughness"]);
  material.occlusion.image = resolve_texture(registry, node["occlusion"]);
  material.emissive.image = resolve_texture(registry, node["emissive"]);
 
  if (node["emissive"] && node["emissive"]["factor"]) {
    const auto& factor = node["emissive"]["factor"];
 
    material.emissive_factor = math::vector4{factor[0].as<std::float_t>(0.0f), factor[1].as<std::float_t>(0.0f), factor[2].as<std::float_t>(0.0f), 1.0f};
  }
 
  if (node["base_color"]) {
    const auto& color = node["base_color"];
 
    material.base_color = math::color{color[0].as<std::float_t>(1.0f), color[1].as<std::float_t>(1.0f), color[2].as<std::float_t>(1.0f), color[3].as<std::float_t>(1.0f)};
  }
 
  material.metallic_factor = node["metallic_factor"].as<std::float_t>(1.0f);
  material.roughness_factor = node["roughness_factor"].as<std::float_t>(1.0f);
  material.specular_factor = node["specular_factor"].as<std::float_t>(1.0f);
 
  material.alpha = parse_alpha_mode(node["alpha_mode"].as<std::string>("opaque"));
  material.alpha_cutoff = node["alpha_cutoff"].as<std::float_t>(0.5f);
 
  material.is_double_sided = node["is_double_sided"].as<bool>(false);
 
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
 
  registry.request_material<models::material>(name, std::move(material));
}

models_module::models_module() {
  auto& scenes_module = core::engine::get_module<sbx::scenes::scenes_module>();
  auto& asset_io = scenes_module.get_asset_io_registry();

  asset_io.register_loader("static_meshes", [](scenes::asset_registry& registry, const utility::hashed_string& name, const YAML::Node& node) -> void {
    const auto path = node["path"].as<std::string>();
    const auto lod_count = node["lod_count"].as<std::uint32_t>(1u);

    auto id = registry.request_mesh<mesh>(name, path, lod_count);
  });

  asset_io.register_loader("materials", [](scenes::asset_registry& registry, const utility::hashed_string& name, const YAML::Node& node) -> void {
    const auto path = node["path"].as<std::string>("");

    if (path.empty()) {
      registry.request_material<material>(name);
      return;
    }

    load_material(registry, name, path);
  });
}

models_module::~models_module() {

}

auto models_module::update() -> void {

}

} // namespace sbx::models
