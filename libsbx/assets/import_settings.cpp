// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/import_settings.hpp>

#include <string>
#include <string_view>

#include <libsbx/utility/hash.hpp>

namespace sbx::assets {

auto default_import_settings(asset_type type) -> import_settings {
  switch (type) {
    case asset_type::texture: return texture_import_settings{};
    case asset_type::mesh: return mesh_import_options{};
    case asset_type::font: return font_import_settings{};
    case asset_type::audio_clip: return audio_clip_import_settings{};
    default: return std::monostate{};
  }
}

auto decode_import_settings(asset_type type, const YAML::Node& node) -> import_settings {
  auto settings = default_import_settings(type);

  if (!node || !node.IsMap()) {
    return settings;
  }

  if (auto* texture = std::get_if<texture_import_settings>(&settings)) {
    texture->generate_mipmaps = node["generate_mipmaps"].as<bool>(texture->generate_mipmaps);
    texture->max_size = node["max_size"].as<std::uint32_t>(texture->max_size);
    texture->flip_y = node["flip_y"].as<bool>(texture->flip_y);
  } else if (auto* mesh = std::get_if<mesh_import_options>(&settings)) {
    mesh->extract_materials = node["extract_materials"].as<bool>(mesh->extract_materials);
    mesh->import_skeleton = node["import_skeleton"].as<bool>(mesh->import_skeleton);
    mesh->included_primitives = node["included_primitives"].as<std::vector<std::size_t>>(mesh->included_primitives);
    mesh->included_animations = node["included_animations"].as<std::vector<std::size_t>>(mesh->included_animations);
    mesh->scale_factor = node["scale_factor"].as<std::float_t>(mesh->scale_factor);
    mesh->generate_lods = node["generate_lods"].as<bool>(mesh->generate_lods);
  } else if (auto* font = std::get_if<font_import_settings>(&settings)) {
    font->reference_size = node["reference_size"].as<std::float_t>(font->reference_size);
    font->first_codepoint = node["first_codepoint"].as<std::uint32_t>(font->first_codepoint);
    font->codepoint_count = node["codepoint_count"].as<std::uint32_t>(font->codepoint_count);
  } else if (auto* audio = std::get_if<audio_clip_import_settings>(&settings)) {
    audio->load_type = reflection::from_string<audio_load_type>(node["load_type"].as<std::string>("")).value_or(audio->load_type);
    audio->force_mono = node["force_mono"].as<bool>(audio->force_mono);
  }

  return settings;
}

auto encode_import_settings(const import_settings& settings) -> YAML::Node {
  auto node = YAML::Node{};

  if (const auto* texture = std::get_if<texture_import_settings>(&settings)) {
    node["generate_mipmaps"] = texture->generate_mipmaps;
    node["max_size"] = texture->max_size;
    node["flip_y"] = texture->flip_y;
  } else if (const auto* mesh = std::get_if<mesh_import_options>(&settings)) {
    node["extract_materials"] = mesh->extract_materials;
    node["import_skeleton"] = mesh->import_skeleton;
    node["included_primitives"] = mesh->included_primitives;
    node["included_animations"] = mesh->included_animations;
    node["scale_factor"] = mesh->scale_factor;
    node["generate_lods"] = mesh->generate_lods;
  } else if (const auto* font = std::get_if<font_import_settings>(&settings)) {
    node["reference_size"] = font->reference_size;
    node["first_codepoint"] = font->first_codepoint;
    node["codepoint_count"] = font->codepoint_count;
  } else if (const auto* audio = std::get_if<audio_clip_import_settings>(&settings)) {
    node["load_type"] = std::string{reflection::to_string(audio->load_type)};
    node["force_mono"] = audio->force_mono;
  }

  return node;
}

auto import_settings_hash(const import_settings& settings) -> std::uint64_t {
  if (std::holds_alternative<std::monostate>(settings)) {
    return 0u;
  }

  auto emitter = YAML::Emitter{};
  emitter << YAML::Flow << encode_import_settings(settings);

  return utility::fnv1a_hash<char, std::uint64_t>{}(std::string_view{emitter.c_str()});
}

} // namespace sbx::assets
