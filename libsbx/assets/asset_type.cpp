// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_type.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace sbx::assets {

static constexpr auto extension_table = std::array<std::pair<std::string_view, asset_type>, 14u>{{
  {".png", asset_type::texture},
  {".jpg", asset_type::texture},
  {".jpeg", asset_type::texture},
  {".gltf", asset_type::mesh},
  {".glb", asset_type::mesh},
  {".material", asset_type::material},
  {".hdr", asset_type::environment_map},
  {".particle_effect", asset_type::particle_effect},
  {".animation_graph", asset_type::animation_graph},
  {".shadergraph", asset_type::shader_graph},
  {".ttf", asset_type::font},
  {".prefab", asset_type::prefab},
  {".scene", asset_type::scene},
  {".cs", asset_type::script},
}};

auto asset_type_of(const std::filesystem::path& path) -> asset_type {
  auto extension = path.extension().string();

  std::ranges::transform(extension, extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

  const auto entry = std::ranges::find(extension_table, std::string_view{extension}, &std::pair<std::string_view, asset_type>::first);

  return entry != extension_table.end() ? entry->second : asset_type::unknown;
}

auto known_extensions() -> std::span<const std::pair<std::string_view, asset_type>> {
  return extension_table;
}

auto is_manifested(asset_type type) -> bool {
  return type != asset_type::unknown && type != asset_type::script;
}

auto has_import_settings(asset_type type) -> bool {
  return type == asset_type::texture || type == asset_type::mesh || type == asset_type::font;
}

auto display_name(asset_type type) -> std::string_view {
  switch (type) {
    case asset_type::texture: return "Texture";
    case asset_type::mesh: return "Mesh";
    case asset_type::material: return "Material";
    case asset_type::environment_map: return "Environment Map";
    case asset_type::particle_effect: return "Particle Effect";
    case asset_type::animation_graph: return "Animation Graph";
    case asset_type::shader_graph: return "Shader Graph";
    case asset_type::font: return "Font";
    case asset_type::prefab: return "Prefab";
    case asset_type::scene: return "Scene";
    case asset_type::script: return "Script";
    case asset_type::unknown: return "Unknown";
  }

  return "Unknown";
}

} // namespace sbx::assets
