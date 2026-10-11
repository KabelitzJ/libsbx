// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/import_settings_editor.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace editor {

// The indices of checked boxes, or empty when all are checked ("empty means everything").
auto all_checked_indices(const std::vector<bool>& checks) -> std::vector<std::size_t> {
  if (std::ranges::all_of(checks, [](bool checked) { return checked; })) {
    return {};
  }

  auto indices = std::vector<std::size_t>{};

  for (auto index = std::size_t{0u}; index < checks.size(); ++index) {
    if (checks[index]) {
      indices.push_back(index);
    }
  }

  return indices;
}

auto checks_from_indices(const std::vector<std::size_t>& indices, std::size_t count) -> std::vector<bool> {
  auto checks = std::vector<bool>(count, indices.empty());

  for (const auto index : indices) {
    if (index < count) {
      checks[index] = true;
    }
  }

  return checks;
}

// One checkbox per entry; returns true if any toggled.
template<typename Label>
auto draw_checklist(std::vector<bool>& checks, Label&& label_of) -> bool {
  auto changed = false;

  for (auto index = std::size_t{0u}; index < checks.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));

    auto checked = static_cast<bool>(checks[index]);

    if (ImGui::Checkbox(label_of(index).c_str(), &checked)) {
      checks[index] = checked;
      changed = true;
    }

    ImGui::PopID();
  }

  return changed;
}

import_settings_editor::import_settings_editor(std::filesystem::path relative_path)
: _relative_path{std::move(relative_path)},
  _type{sbx::assets::asset_type_of(_relative_path)} {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto absolute_path = project.assets_directory() / _relative_path;

  auto meta_path = absolute_path;
  meta_path += ".meta";

  // A file without a .meta isn't imported yet; apply() imports it, so Skip leaves nothing behind.
  if (std::filesystem::exists(meta_path)) {
    _id = assets_module.import(absolute_path);
    _settings = assets_module.import_settings_of(_id);
  } else {
    _settings = sbx::assets::default_import_settings(_type);
  }

  _applied = _settings;

  if (_type == sbx::assets::asset_type::mesh) {
    _mesh_summary = sbx::assets::asset_cooker::inspect_mesh_source(absolute_path);
  }

  _sync_checks_from_settings();
}

auto import_settings_editor::draw() -> bool {
  if (auto* texture = std::get_if<sbx::assets::texture_import_settings>(&_settings)) {
    return _draw_texture(*texture);
  }

  if (auto* mesh = std::get_if<sbx::assets::mesh_import_options>(&_settings)) {
    return _draw_mesh(*mesh);
  }

  if (auto* font = std::get_if<sbx::assets::font_import_settings>(&_settings)) {
    return _draw_font(*font);
  }

  if (auto* audio = std::get_if<sbx::assets::audio_clip_import_settings>(&_settings)) {
    return _draw_audio_clip(*audio);
  }

  ImGui::TextDisabled("This asset type has no import settings.");

  return false;
}

auto import_settings_editor::apply() -> sbx::math::uuid {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (_id == sbx::math::uuid::nil()) {
    _id = assets_module.import(project.assets_directory() / _relative_path);
  }

  assets_module.set_import_settings(_id, _settings);
  assets_module.reimport(_id);

  // Cooking a mesh is what extracts its .material files, so a fresh import cooks right away instead of on first use.
  if (_type == sbx::assets::asset_type::mesh) {
    assets_module.load_mesh(_id);
  }

  _applied = _settings;

  return _id;
}

auto import_settings_editor::adopt(const sbx::assets::import_settings& settings) -> void {
  if (settings.index() != _settings.index()) {
    return;
  }

  _settings = settings;

  if (auto* mesh = std::get_if<sbx::assets::mesh_import_options>(&_settings)) {
    mesh->included_primitives.clear();
    mesh->included_animations.clear();
  }

  _sync_checks_from_settings();
}

auto import_settings_editor::_sync_checks_from_settings() -> void {
  const auto* mesh = std::get_if<sbx::assets::mesh_import_options>(&_settings);

  if (mesh == nullptr || !_mesh_summary.has_value()) {
    return;
  }

  _primitive_checks = checks_from_indices(mesh->included_primitives, _mesh_summary->primitives.size());
  _animation_checks = checks_from_indices(mesh->included_animations, _mesh_summary->animation_names.size());
}

auto import_settings_editor::_draw_texture(sbx::assets::texture_import_settings& settings) -> bool {
  static constexpr auto max_size_values = std::array<std::uint32_t, 7u>{0u, 256u, 512u, 1024u, 2048u, 4096u, 8192u};
  static constexpr auto max_size_names = std::array<const char*, 7u>{"Source Size", "256", "512", "1024", "2048", "4096", "8192"};

  auto changed = ImGui::Checkbox("Generate Mipmaps", &settings.generate_mipmaps);

  auto max_size_index = static_cast<std::int32_t>(std::ranges::find(max_size_values, settings.max_size) - max_size_values.begin());

  // A hand-edited .meta value that isn't in the list shows as Source Size until changed.
  if (max_size_index >= static_cast<std::int32_t>(max_size_values.size())) {
    max_size_index = 0;
  }

  if (ImGui::Combo("Max Size", &max_size_index, max_size_names.data(), static_cast<std::int32_t>(max_size_names.size()))) {
    settings.max_size = max_size_values[static_cast<std::size_t>(max_size_index)];
    changed = true;
  }

  changed |= ImGui::Checkbox("Flip Vertically", &settings.flip_y);

  return changed;
}

auto import_settings_editor::_draw_mesh(sbx::assets::mesh_import_options& settings) -> bool {
  auto changed = false;

  if (ImGui::DragFloat("Scale Factor", &settings.scale_factor, 0.01f, 0.0001f, 10000.0f, "%.4g", ImGuiSliderFlags_Logarithmic)) {
    settings.scale_factor = std::max(settings.scale_factor, 0.0001f);
    changed = true;
  }

  changed |= ImGui::Checkbox("Generate LODs", &settings.generate_lods);

  changed |= ImGui::Checkbox("Extract Materials", &settings.extract_materials);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Extracts embedded materials into editable .material assets. When off, they're cooked read-only and won't appear in the Asset Browser.");
  }

  if (!_mesh_summary.has_value()) {
    ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, "Could not read this file's contents.");
    return changed;
  }

  const auto& summary = *_mesh_summary;

  if (ImGui::TreeNodeEx("Primitives", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (draw_checklist(_primitive_checks, [&](std::size_t index) { return std::string{summary.primitives[index].mesh_name} + " [" + std::to_string(summary.primitives[index].primitive_index) + "]"; })) {
      settings.included_primitives = all_checked_indices(_primitive_checks);
      changed = true;
    }

    ImGui::TreePop();
  }

  ImGui::BeginDisabled(!summary.has_skeleton);
  changed |= ImGui::Checkbox("Import Skeleton", &settings.import_skeleton);
  ImGui::EndDisabled();

  if (!summary.has_skeleton) {
    ImGui::TextDisabled("This file has no skeleton.");
  } else if (!summary.animation_names.empty()) {
    ImGui::BeginDisabled(!settings.import_skeleton);

    if (ImGui::TreeNodeEx("Animation Clips", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (draw_checklist(_animation_checks, [&](std::size_t index) { return summary.animation_names[index]; })) {
        settings.included_animations = all_checked_indices(_animation_checks);
        changed = true;
      }

      ImGui::TreePop();
    }

    ImGui::EndDisabled();
  }

  return changed;
}

auto import_settings_editor::_draw_font(sbx::assets::font_import_settings& settings) -> bool {
  auto changed = ImGui::DragFloat("Atlas Glyph Size", &settings.reference_size, 0.5f, 8.0f, 256.0f, "%.0f px");

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("The raster height glyphs are baked at. Larger stays sharper at big sizes but costs atlas memory.");
  }

  auto first = static_cast<std::int32_t>(settings.first_codepoint);
  auto count = static_cast<std::int32_t>(settings.codepoint_count);

  if (ImGui::InputInt("First Codepoint", &first)) {
    settings.first_codepoint = static_cast<std::uint32_t>(std::clamp(first, 0, 0x10FFFF));
    changed = true;
  }

  if (ImGui::InputInt("Codepoint Count", &count)) {
    settings.codepoint_count = static_cast<std::uint32_t>(std::clamp(count, 1, 0xFFFF));
    changed = true;
  }

  ImGui::TextDisabled("U+%04X to U+%04X", settings.first_codepoint, settings.first_codepoint + settings.codepoint_count - 1u);

  return changed;
}

auto import_settings_editor::_draw_audio_clip(sbx::assets::audio_clip_import_settings& settings) -> bool {
  auto changed = false;

  auto extension = _relative_path.extension().string();
  std::ranges::transform(extension, extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

  const auto is_vorbis = extension == ".ogg";

  ImGui::BeginDisabled(!is_vorbis);

  auto compressed = settings.load_type == sbx::assets::audio_load_type::compressed_in_memory;

  if (ImGui::Checkbox("Compressed In Memory", &compressed)) {
    settings.load_type = compressed ? sbx::assets::audio_load_type::compressed_in_memory : sbx::assets::audio_load_type::decompress_on_load;
    changed = true;
  }

  ImGui::EndDisabled();

  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(is_vorbis ? "Keeps the Ogg Vorbis data and decodes it while playing. Use for music and long ambience." : "Only Ogg Vorbis clips can stay compressed; other formats always decompress on load.");
  }

  changed |= ImGui::Checkbox("Force To Mono", &settings.force_mono);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Downmixes to one channel. Needed for 3D (spatial) playback of stereo files.");
  }

  return changed;
}

} // namespace editor
