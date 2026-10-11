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

auto filename_less(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool {
  const auto to_lower = [](const std::filesystem::path& path) {
    auto name = path.filename().string();
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name;
  };

  return to_lower(lhs) < to_lower(rhs);
}

auto importable_extensions() -> std::vector<std::string> {
  auto extensions = std::vector<std::string>{};

  for (const auto& [extension, type] : sbx::assets::known_extensions()) {
    if (sbx::assets::is_manifested(type)) {
      extensions.emplace_back(extension);
    }
  }

  return extensions;
}

auto icon_for(const asset_browser_entry& entry) -> const char* {
  return entry.is_directory ? ICON_MDI_FOLDER : sbx::render::icon_for(entry.kind);
}

auto is_entry_selected(const editor_state& state, const std::filesystem::path& path) -> bool {
  const auto* selected = std::get_if<asset_selection>(&state.current_selection);
  return selected != nullptr && selected->path == path;
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
