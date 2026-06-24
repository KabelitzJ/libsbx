// SPDX-License-Identifier: MIT
#include <libsbx/models/material_serializer.hpp>

#include <fstream>

#include <libsbx/assets/serializer_registry.hpp>

namespace sbx::models {

auto material_serializer::type() const -> std::string_view {
  return models::material::type_name;
}

auto material_serializer::read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset_base> {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto node = YAML::LoadFile(context.resolved.string());

  auto material = models::material{};

  material.name = node["name"].as<std::string>("Material");
  material.albedo = _resolve_texture(assets_module, node["albedo"]);
  material.normal = _resolve_texture(assets_module, node["normal"]);
  material.metallic_roughness = _resolve_texture(assets_module, node["metallic_roughness"]);
  material.occlusion = _resolve_texture(assets_module, node["occlusion"]);
  material.emissive = _resolve_texture(assets_module, node["emissive"]);
  material.height = _resolve_texture(assets_module, node["height"]);

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

auto material_serializer::write(const assets::serializer_context& context, const std::unique_ptr<assets::asset_base>& asset) -> bool {
  const auto* material = dynamic_cast<const models::material*>(asset.get());

  if (!material) {
    return false;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto node = YAML::Node{};

  node["name"] = material->name;

  auto base_color = YAML::Node{};
  base_color.SetStyle(YAML::EmitterStyle::Flow);
  base_color.push_back(material->base_color.r());
  base_color.push_back(material->base_color.g());
  base_color.push_back(material->base_color.b());
  base_color.push_back(material->base_color.a());
  node["base_color"] = base_color;

  node["metallic_factor"] = material->metallic_factor;
  node["roughness_factor"] = material->roughness_factor;
  node["specular_factor"] = material->specular_factor;

  node["alpha_mode"] = _alpha_mode_name(material->alpha);
  node["alpha_cutoff"] = material->alpha_cutoff;
  node["is_double_sided"] = material->is_double_sided;

  _encode_texture(assets_module, node, "albedo", material->albedo);
  _encode_texture(assets_module, node, "normal", material->normal);
  _encode_texture(assets_module, node, "metallic_roughness", material->metallic_roughness);
  _encode_texture(assets_module, node, "occlusion", material->occlusion);
  _encode_texture(assets_module, node, "emissive", material->emissive);
  _encode_texture(assets_module, node, "height", material->height);

  if (material->emissive_factor != math::vector4{0.0f, 0.0f, 0.0f, 1.0f}) {
    auto emissive = node["emissive"] ? node["emissive"] : YAML::Node{};

    auto factor = YAML::Node{};
    factor.SetStyle(YAML::EmitterStyle::Flow);
    factor.push_back(material->emissive_factor.x());
    factor.push_back(material->emissive_factor.y());
    factor.push_back(material->emissive_factor.z());

    emissive["factor"] = factor;
    node["emissive"] = emissive;
  }

  if (material->sway_speed != 0.0f || material->sway_strength != 0.0f) {
    auto sway = YAML::Node{};
    sway["speed"] = material->sway_speed;
    sway["strength"] = material->sway_strength;
    sway["falloff_exponent"] = material->sway_falloff_exponent;
    node["sway"] = sway;
  }

  if (material->scrumble_speed != 0.0f || material->scrumble_strength != 0.0f) {
    auto scrumble = YAML::Node{};
    scrumble["speed"] = material->scrumble_speed;
    scrumble["strength"] = material->scrumble_strength;
    scrumble["falloff_exponent"] = material->scrumble_falloff_exponent;
    node["scrumble"] = scrumble;
  }

  std::filesystem::create_directories(context.resolved.parent_path());

  auto file = std::ofstream{context.resolved};

  if (!file) {
    return false;
  }

  file << YAML::Dump(node);

  return true;
}

auto material_serializer::_resolve_texture(assets::assets_module& assets_module, const YAML::Node& node) -> texture_slot {
  auto slot = texture_slot{};

  if (!node || !node["image"]) {
    return slot;
  }

  const auto image_path = node["image"].as<std::string>();
  const auto format = node["format"].as<std::string>("r8g8b8a8_unorm");

  auto settings = YAML::Node{};

  settings["srgb"] = format.contains("srgb");

  slot.texture = assets_module.load_asset(image_path, settings);
  slot.image = assets_module.get_loaded<graphics::texture>(slot.texture).handle();

  return slot;
}

auto material_serializer::_encode_texture(assets::assets_module& assets_module, YAML::Node& parent, const std::string& key, const texture_slot& slot) -> void {
  if (slot.texture == math::uuid::nil()) {
    return;
  }

  const auto source = assets_module.source_of(slot.texture);

  if (!source || source->empty()) {
    return;
  }

  auto node = YAML::Node{};

  node["image"] = source->generic_string();

  parent[key] = node;
}

auto material_serializer::_parse_alpha_mode(const std::string& value) -> models::alpha_mode {
  if (value == "mask") {
    return models::alpha_mode::mask;
  }

  if (value == "blend") {
    return models::alpha_mode::blend;
  }

  return models::alpha_mode::opaque;
}

auto material_serializer::_alpha_mode_name(const models::alpha_mode value) -> std::string {
  switch (value) {
    case models::alpha_mode::mask: {
      return "mask";
    }
    case models::alpha_mode::blend: {
      return "blend";
    }
    default: {
      return "opaque";
    }
  }
}

} // namespace sbx::models
