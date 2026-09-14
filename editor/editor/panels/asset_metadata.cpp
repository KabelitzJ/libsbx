// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_metadata.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>

#include <fmt/format.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>
#include <libsbx/render/ui/widgets/asset_tile.hpp>

namespace editor {

auto extension_table() -> const std::unordered_map<std::string, asset_kind>& {
  static const auto table = std::unordered_map<std::string, asset_kind>{
    {".png", asset_kind::texture},
    {".jpg", asset_kind::texture},
    {".jpeg", asset_kind::texture},
    {".gltf", asset_kind::mesh},
    {".glb", asset_kind::mesh},
    {".material", asset_kind::material},
    {".hdr", asset_kind::environment_map},
    {".particle_effect", asset_kind::particle_effect},
    {".animation_graph", asset_kind::animation_graph},
    {".shadergraph", asset_kind::shader_graph},
    {".ttf", asset_kind::font},
    {".prefab", asset_kind::prefab},
    {".yaml", asset_kind::scene},
    {".cs", asset_kind::script},
  };

  return table;
}

auto is_importable_kind(asset_kind kind) -> bool {
  return kind == asset_kind::texture || kind == asset_kind::mesh ||
         kind == asset_kind::material || kind == asset_kind::environment_map ||
         kind == asset_kind::particle_effect || kind == asset_kind::animation_graph ||
         kind == asset_kind::shader_graph || kind == asset_kind::font;
}

auto filename_less(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool {
  const auto to_lower = [](const std::filesystem::path& path) {
    auto name = path.filename().string();
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name;
  };

  return to_lower(lhs) < to_lower(rhs);
}

auto classify_extension(const std::filesystem::path& extension) -> asset_kind {
  const auto& table = extension_table();
  const auto entry = table.find(extension.string());

  return entry != table.end() ? entry->second : asset_kind::unknown;
}

auto importable_extensions() -> std::vector<std::string> {
  auto extensions = std::vector<std::string>{};

  for (const auto& [extension, kind] : extension_table()) {
    if (is_importable_kind(kind)) {
      extensions.push_back(extension);
    }
  }

  return extensions;
}

auto icon_for(const asset_browser_entry& entry) -> const char* {
  if (entry.is_directory) {
    return ICON_MDI_FOLDER;
  }

  switch (entry.kind) {
    case asset_kind::texture: return ICON_MDI_IMAGE;
    case asset_kind::mesh: return ICON_MDI_CUBE_OUTLINE;
    case asset_kind::material: return ICON_MDI_PALETTE_SWATCH;
    case asset_kind::environment_map: return ICON_MDI_EARTH;
    case asset_kind::particle_effect: return ICON_MDI_FIREWORK;
    case asset_kind::animation_graph: return ICON_MDI_STATE_MACHINE;
    case asset_kind::shader_graph: return ICON_MDI_VECTOR_POLYLINE;
    case asset_kind::font: return ICON_MDI_FORMAT_FONT;
    case asset_kind::prefab: return ICON_MDI_CUBE_SCAN;
    case asset_kind::scene: return ICON_MDI_FILE_TREE;
    case asset_kind::script: return ICON_MDI_FILE_CODE_OUTLINE;
    case asset_kind::unknown: return ICON_MDI_FILE_OUTLINE;
  }

  return ICON_MDI_FILE_OUTLINE;
}

auto is_entry_selected(const editor_state& state, const std::filesystem::path& path) -> bool {
  const auto* selected = std::get_if<asset_selection>(&state.current_selection);
  return selected != nullptr && selected->path == path;
}

auto drag_payload_type_for(asset_kind kind) -> const char* {
  switch (kind) {
    case asset_kind::texture: return sbx::render::drag_drop_payload_texture;
    case asset_kind::mesh: return sbx::render::drag_drop_payload_mesh;
    case asset_kind::material: return sbx::render::drag_drop_payload_material;
    case asset_kind::particle_effect: return sbx::render::drag_drop_payload_particle_effect;
    case asset_kind::animation_graph: return sbx::render::drag_drop_payload_animation_graph;
    case asset_kind::shader_graph: return sbx::render::drag_drop_payload_shader_graph;
    case asset_kind::font: return sbx::render::drag_drop_payload_font;
    case asset_kind::prefab: return sbx::render::drag_drop_payload_prefab;
    default: return nullptr;
  }
}

auto make_move_drag_payload(const std::filesystem::path& relative_path) -> asset_move_drag_payload {
  auto payload = asset_move_drag_payload{};

  const auto path_string = relative_path.string();
  const auto copy_length = std::min(path_string.size(), sizeof(payload.path) - 1u);
  std::memcpy(payload.path, path_string.data(), copy_length);
  payload.path[copy_length] = '\0';

  return payload;
}

auto path_from_move_payload(const ImGuiPayload& payload) -> std::filesystem::path {
  return std::filesystem::path{static_cast<const asset_move_drag_payload*>(payload.Data)->path};
}

auto truncate_to_width(const std::string& text, std::float_t max_width) -> std::string {
  if (ImGui::CalcTextSize(text.c_str()).x <= max_width) {
    return text;
  }

  static constexpr auto ellipsis = std::string_view{"..."};

  auto truncated = text;

  while (!truncated.empty() && ImGui::CalcTextSize((truncated + std::string{ellipsis}).c_str()).x > max_width) {
    truncated.pop_back();
  }

  return truncated + std::string{ellipsis};
}

auto is_hidden_from_browser(const std::filesystem::path& name, bool is_directory) -> bool {
  const auto name_string = name.string();
  const auto name_extension = name.extension();

  if (!name_string.empty() && name_string.front() == '.') {
    return true;
  }

  if (is_directory) {
    return name_string == "bin" || name_string == "obj";
  }

  return name_extension == ".meta" || name_extension == ".csproj";
}

auto has_visible_subdirectories(const std::filesystem::path& directory) -> bool {
  auto ec = std::error_code{};

  for (const auto& entry : std::filesystem::directory_iterator{directory, ec}) {
    if (entry.is_directory(ec) && !is_hidden_from_browser(entry.path().filename(), true)) {
      return true;
    }
  }

  return false;
}

auto is_ancestor_or_self(const std::filesystem::path& candidate, const std::filesystem::path& target) -> bool {
  auto candidate_it = candidate.begin();
  auto target_it = target.begin();

  for (; candidate_it != candidate.end(); ++candidate_it, ++target_it) {
    if (target_it == target.end() || *candidate_it != *target_it) {
      return false;
    }
  }

  return true;
}

auto filter_matches(std::string_view name, std::string_view filter) -> bool {
  if (filter.empty()) {
    return true;
  }

  const auto to_lower = [](std::string_view text) {
    auto result = std::string{text};
    std::ranges::transform(result, result.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
  };

  return to_lower(name).find(to_lower(filter)) != std::string::npos;
}

auto unique_name(const std::filesystem::path& absolute_directory, std::string_view stem, std::string_view extension) -> std::string {
  auto name = extension.empty() ? std::string{stem} : fmt::format("{}{}", stem, extension);

  if (!std::filesystem::exists(absolute_directory / name)) {
    return name;
  }

  for (auto suffix = 1;; ++suffix) {
    name = extension.empty() ? fmt::format("{} {}", stem, suffix) : fmt::format("{} {}{}", stem, suffix, extension);

    if (!std::filesystem::exists(absolute_directory / name)) {
      return name;
    }
  }
}

} // namespace editor
