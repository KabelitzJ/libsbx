// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_preferences.hpp>

#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/reflection/enum.hpp>

#include <editor/editor_module.hpp>

namespace editor {

auto editor_preferences::load(const std::filesystem::path& path) -> editor_preferences {
  auto result = editor_preferences{};

  if (!std::filesystem::exists(path)) {
    return result;
  }

  const auto root = YAML::LoadFile(path.string());

  if (root["axis_colors"]) {
    result.axis_colors = sbx::reflection::from_string_or(root["axis_colors"].as<std::string>(), result.axis_colors);
  }

  if (root["translate_snap"]) {
    result.translate_snap = root["translate_snap"].as<std::float_t>();
  }

  if (root["rotate_snap"]) {
    result.rotate_snap = root["rotate_snap"].as<std::float_t>();
  }

  if (root["scale_snap"]) {
    result.scale_snap = root["scale_snap"].as<std::float_t>();
  }

  if (root["interface_scale"]) {
    result.interface_scale = root["interface_scale"].as<std::float_t>();
  }

  if (const auto recent = root["recent_scenes"]; recent && recent.IsSequence()) {
    for (const auto path : recent) {
      result.recent_scenes.push_back(path.as<std::string>());
    }
  }

  if (const auto shortcuts = root["shortcuts"]; shortcuts && shortcuts.IsMap()) {
    for (const auto entry : shortcuts) {
      auto chords = std::vector<std::string>{};

      for (const auto chord : entry.second) {
        chords.push_back(chord.as<std::string>());
      }

      result.shortcut_overrides.emplace(entry.first.as<std::string>(), std::move(chords));
    }
  }

  return result;
}

auto editor_preferences::save(const std::filesystem::path& path) const -> void {
  auto root = YAML::Node{};

  root["axis_colors"] = std::string{sbx::reflection::to_string(axis_colors)};
  root["translate_snap"] = translate_snap;
  root["rotate_snap"] = rotate_snap;
  root["scale_snap"] = scale_snap;
  root["interface_scale"] = interface_scale;

  if (!recent_scenes.empty()) {
    auto recent = YAML::Node{YAML::NodeType::Sequence};

    for (const auto& path : recent_scenes) {
      recent.push_back(path);
    }

    root["recent_scenes"] = recent;
  }

  if (!shortcut_overrides.empty()) {
    auto shortcuts = YAML::Node{YAML::NodeType::Map};

    for (const auto& [id, chords] : shortcut_overrides) {
      auto list = YAML::Node{YAML::NodeType::Sequence};

      for (const auto& chord : chords) {
        list.push_back(chord);
      }

      shortcuts[id] = list;
    }

    root["shortcuts"] = shortcuts;
  }

  std::filesystem::create_directories(path.parent_path());

  auto out = std::ofstream{path};
  out << root;
}

auto effective_interface_scale() -> std::float_t {
  const auto preference = sbx::core::engine::get_module<editor_module>().preferences().interface_scale;

  if (preference > 0.0f) {
    return preference;
  }

  return std::max(ImGui::GetMainViewport()->DpiScale, 1.0f);
}

auto axis_colors() -> const axis_palette& {
  const auto scheme = sbx::core::engine::get_module<editor_module>().preferences().axis_colors;

  return (scheme == axis_color_scheme::vivid) ? axis_palette_vivid : axis_palette_muted;
}

} // namespace editor
