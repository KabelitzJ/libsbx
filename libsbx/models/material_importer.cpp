// SPDX-License-Identifier: MIT
#include <libsbx/models/material_importer.hpp>

#include <libsbx/assets/importer_registry.hpp>

namespace sbx::models {

auto material_importer::type() const -> std::string_view {
  return models::material::type_name;
}

auto material_importer::import(const assets::import_context& context) -> std::unique_ptr<assets::asset_base> {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto node = YAML::LoadFile(context.resolved.string());

  auto material = models::material{};

  material.albedo.image = _resolve_texture(assets_module, material.texture_dependencies, node["albedo"]);
  material.normal.image = _resolve_texture(assets_module, material.texture_dependencies, node["normal"]);
  material.metallic_roughness.image = _resolve_texture(assets_module, material.texture_dependencies, node["metallic_roughness"]);
  material.occlusion.image = _resolve_texture(assets_module, material.texture_dependencies, node["occlusion"]);
  material.emissive.image = _resolve_texture(assets_module, material.texture_dependencies, node["emissive"]);
  material.height.image = _resolve_texture(assets_module, material.texture_dependencies, node["height"]);

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

  material.alpha = _parse_alpha_mode(node["alpha_mode"].as<std::string>("opaque"));
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
    material.scrumble_falloff_exponent = scrumble["falloff_exponent"].as<std::float_t>(2.0f);
  }

  if (node["surface_shader"]) {
    const auto& surface_shader = node["surface_shader"];

    auto shader_path = surface_shader["path"].as<std::string>();
    constexpr auto prefix = std::string_view{"res://"};

    if (shader_path.starts_with(prefix)) {
      shader_path.erase(0, prefix.length());
    }

    material.surface_shader = shader_path;

    if (surface_shader["required_attributes"]) {
      for (const auto& attribute : surface_shader["required_attributes"]) {
        const auto value = reflection::from_string<models::vertex_stream>(attribute.as<std::string>());

        if (value) {
          material.required_streams.set(*value);
        }
      }
    }
  }

  return std::make_unique<models::material>(std::move(material));
}

auto material_importer::_resolve_texture(assets::assets_module& assets_module, std::vector<math::uuid>& dependencies, const YAML::Node& node) -> graphics::image2d_handle {
  if (!node || !node["image"]) {
    return graphics::image2d_handle{};
  }

  const auto image_path = node["image"].as<std::string>();
  const auto format = node["format"].as<std::string>("r8g8b8a8_unorm");

  auto settings = YAML::Node{};

  settings["srgb"] = format.contains("srgb");

  const auto id = assets_module.load_asset(image_path, settings);

  dependencies.push_back(id);

  return assets_module.get_loaded<graphics::texture>(id).handle();
}

auto material_importer::_parse_alpha_mode(const std::string& value) -> models::alpha_mode {
  if (value == "mask") {
    return models::alpha_mode::mask;
  }

  if (value == "blend") {
    return models::alpha_mode::blend;
  }

  return models::alpha_mode::opaque;
}

const auto material_importer_registered = sbx::assets::register_importer<sbx::models::material_importer>({".material.yaml"});

} // namespace sbx::models
