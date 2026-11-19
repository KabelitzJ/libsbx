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
    mesh->lod_count = node["lod_count"].as<std::uint32_t>(mesh->lod_count);
    mesh->lod_reduction = node["lod_reduction"].as<std::float_t>(mesh->lod_reduction);
    mesh->lod_max_error = node["lod_max_error"].as<std::float_t>(mesh->lod_max_error);
    mesh->lod_bias = node["lod_bias"].as<std::float_t>(mesh->lod_bias);

    if (const auto clips = node["clip_events"]) {
      for (const auto clip_node : clips) {
        auto clip = animation_clip_events{};
        clip.clip_name = clip_node["clip"].as<std::string>("");

        if (const auto events = clip_node["events"]) {
          for (const auto event_node : events) {
            auto event = animation_event{};
            event.time = event_node["time"].as<std::float_t>(event.time);
            event.name = event_node["name"].as<std::string>("");
            event.float_parameter = event_node["float"].as<std::float_t>(event.float_parameter);
            event.int_parameter = event_node["int"].as<std::int32_t>(event.int_parameter);
            event.string_parameter = event_node["string"].as<std::string>("");
            event.min_weight = event_node["min_weight"].as<std::float_t>(event.min_weight);
            clip.events.push_back(std::move(event));
          }
        }

        mesh->clip_events.push_back(std::move(clip));
      }
    }
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

    // Written only when changed, so .meta files and cook hashes from before these existed stay as they were.
    const auto defaults = mesh_import_options{};

    if (mesh->lod_count != defaults.lod_count) node["lod_count"] = mesh->lod_count;
    if (mesh->lod_reduction != defaults.lod_reduction) node["lod_reduction"] = mesh->lod_reduction;
    if (mesh->lod_max_error != defaults.lod_max_error) node["lod_max_error"] = mesh->lod_max_error;
    if (mesh->lod_bias != defaults.lod_bias) node["lod_bias"] = mesh->lod_bias;

    // Written only when there are any, so .meta files and hashes from before events stay as they were.
    if (!mesh->clip_events.empty()) {
      auto clips = YAML::Node{YAML::NodeType::Sequence};

      for (const auto& clip : mesh->clip_events) {
        auto clip_node = YAML::Node{};
        clip_node["clip"] = clip.clip_name;

        auto events = YAML::Node{YAML::NodeType::Sequence};

        for (const auto& event : clip.events) {
          auto event_node = YAML::Node{};
          event_node["time"] = event.time;
          event_node["name"] = event.name;
          event_node["float"] = event.float_parameter;
          event_node["int"] = event.int_parameter;
          event_node["string"] = event.string_parameter;
          event_node["min_weight"] = event.min_weight;
          events.push_back(event_node);
        }

        clip_node["events"] = events;
        clips.push_back(clip_node);
      }

      node["clip_events"] = clips;
    }
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

  // Clip events never reach the cooked data.
  auto hashed = settings;

  if (auto* mesh = std::get_if<mesh_import_options>(&hashed)) {
    mesh->clip_events.clear();
    mesh->lod_bias = mesh_import_options{}.lod_bias;
  }

  auto emitter = YAML::Emitter{};
  emitter << YAML::Flow << encode_import_settings(hashed);

  return utility::fnv1a_hash<char, std::uint64_t>{}(std::string_view{emitter.c_str()});
}

} // namespace sbx::assets
