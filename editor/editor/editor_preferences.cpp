// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_preferences.hpp>

#include <fstream>
#include <string>

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

  return result;
}

auto editor_preferences::save(const std::filesystem::path& path) const -> void {
  auto root = YAML::Node{};

  root["axis_colors"] = std::string{sbx::reflection::to_string(axis_colors)};
  root["translate_snap"] = translate_snap;
  root["rotate_snap"] = rotate_snap;
  root["scale_snap"] = scale_snap;

  std::filesystem::create_directories(path.parent_path());

  auto out = std::ofstream{path};
  out << root;
}

auto axis_colors() -> const axis_palette& {
  const auto scheme = sbx::core::engine::get_module<editor_module>().preferences().axis_colors;

  return (scheme == axis_color_scheme::vivid) ? axis_palette_vivid : axis_palette_muted;
}

} // namespace editor
