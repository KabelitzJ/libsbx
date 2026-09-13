// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/widgets/vector_fields.hpp>

#include <optional>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

namespace editor::widgets {

auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, std::float_t reset_value, std::float_t speed) -> vector3_edit_result {
  static constexpr auto axis_labels = std::array<const char*, 3u>{"X", "Y", "Z"};
  static constexpr auto axis_ids = std::array<const char*, 3u>{"##X", "##Y", "##Z"};
  static constexpr auto axis_colors = std::array<ImVec4, 3u>{
    ImVec4{0.75f, 0.20f, 0.25f, 1.0f}, // X - red
    ImVec4{0.30f, 0.65f, 0.30f, 1.0f}, // Y - green
    ImVec4{0.20f, 0.45f, 0.80f, 1.0f}, // Z - blue
  };

  auto result = vector3_edit_result{};

  ImGui::PushID(label);

  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(90.0f);

  const auto line_height = ImGui::GetFrameHeight();
  const auto button_size = ImVec2{line_height, line_height};
  const auto item_width = (ImGui::GetContentRegionAvail().x - 3.0f * button_size.x) / 3.0f - 2.0f * ImGui::GetStyle().ItemSpacing.x;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{2.0f, 0.0f});

  for (auto axis = std::size_t{0u}; axis < 3u; ++axis) {
    if (axis != 0u) {
      ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, axis_colors[axis]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, axis_colors[axis]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, axis_colors[axis]);

    if (ImGui::Button(axis_labels[axis], button_size)) {
      values[axis] = reset_value;
      result.changed = true;
      result.started = true;
      result.committed = true;
    }

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(item_width);
    result.changed |= ImGui::DragFloat(axis_ids[axis], &values[axis], speed);
    result.started |= ImGui::IsItemActivated();
    result.committed |= ImGui::IsItemDeactivatedAfterEdit();
  }

  ImGui::PopStyleVar();
  ImGui::PopID();

  return result;
}

auto draw_vector2_control(const char* label, std::array<std::float_t, 2u>& values, std::float_t reset_value, std::float_t speed) -> vector2_edit_result {
  static constexpr auto axis_labels = std::array<const char*, 2u>{"X", "Y"};
  static constexpr auto axis_ids = std::array<const char*, 2u>{"##X", "##Y"};
  static constexpr auto axis_colors = std::array<ImVec4, 2u>{
    ImVec4{0.75f, 0.20f, 0.25f, 1.0f}, // X - red
    ImVec4{0.30f, 0.65f, 0.30f, 1.0f}, // Y - green
  };

  auto result = vector2_edit_result{};

  ImGui::PushID(label);

  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(90.0f);

  const auto line_height = ImGui::GetFrameHeight();
  const auto button_size = ImVec2{line_height, line_height};
  const auto item_width = (ImGui::GetContentRegionAvail().x - 2.0f * button_size.x) / 2.0f - 1.0f * ImGui::GetStyle().ItemSpacing.x;

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{2.0f, 0.0f});

  for (auto axis = std::size_t{0u}; axis < 2u; ++axis) {
    if (axis != 0u) {
      ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, axis_colors[axis]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, axis_colors[axis]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, axis_colors[axis]);

    if (ImGui::Button(axis_labels[axis], button_size)) {
      values[axis] = reset_value;
      result.changed = true;
      result.started = true;
      result.committed = true;
    }

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(item_width);
    result.changed |= ImGui::DragFloat(axis_ids[axis], &values[axis], speed);
    result.started |= ImGui::IsItemActivated();
    result.committed |= ImGui::IsItemDeactivatedAfterEdit();
  }

  ImGui::PopStyleVar();
  ImGui::PopID();

  return result;
}

auto draw_color_field(const char* label, sbx::math::color& color) -> bool {
  auto value = std::array<std::float_t, 4u>{color.r(), color.g(), color.b(), color.a()};

  if (!ImGui::ColorEdit4(label, value.data())) {
    return false;
  }

  color.r() = value[0];
  color.g() = value[1];
  color.b() = value[2];
  color.a() = value[3];

  return true;
}

auto draw_curve_editor(const char* label, sbx::assets::curve& curve, std::float_t value_min, std::float_t value_max) -> bool {
  auto changed = false;

  if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Framed)) {
    return false;
  }

  auto& keys = curve.keys;
  auto removed_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < keys.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& key = keys[index];

    changed |= ImGui::SliderFloat("Time", &key.time, 0.0f, 1.0f);
    changed |= ImGui::DragFloat("Value", &key.value, 0.01f, value_min, value_max);

    if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Key")) {
      removed_index = index;
      changed = true;
    }

    ImGui::Separator();
    ImGui::PopID();
  }

  if (removed_index) {
    // static_vector has no erase() -- shift the tail down by hand, same as the collision plane list.
    for (auto i = *removed_index; i + 1u < keys.size(); ++i) {
      keys[i] = keys[i + 1u];
    }

    keys.pop_back();
  }

  ImGui::BeginDisabled(keys.size() >= sbx::assets::curve_max_keys);

  if (ImGui::Button(ICON_MDI_PLUS " Add Key")) {
    keys.push_back(sbx::assets::curve_key{});
    changed = true;
  }

  ImGui::EndDisabled();

  ImGui::TreePop();

  return changed;
}

auto draw_gradient_editor(const char* label, sbx::assets::gradient& gradient) -> bool {
  auto changed = false;

  if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Framed)) {
    return false;
  }

  ImGui::Text("Color Keys");
  ImGui::PushID("color_keys");

  auto& color_keys = gradient.color_keys;
  auto removed_color_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < color_keys.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& key = color_keys[index];

    changed |= ImGui::SliderFloat("Time", &key.time, 0.0f, 1.0f);
    changed |= draw_color_field("Color", key.color);

    if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Color Key")) {
      removed_color_index = index;
      changed = true;
    }

    ImGui::Separator();
    ImGui::PopID();
  }

  if (removed_color_index) {
    for (auto i = *removed_color_index; i + 1u < color_keys.size(); ++i) {
      color_keys[i] = color_keys[i + 1u];
    }

    color_keys.pop_back();
  }

  ImGui::BeginDisabled(color_keys.size() >= sbx::assets::gradient_max_keys);

  if (ImGui::Button(ICON_MDI_PLUS " Add Color Key")) {
    color_keys.push_back(sbx::assets::gradient_color_key{});
    changed = true;
  }

  ImGui::EndDisabled();
  ImGui::PopID();

  ImGui::Text("Alpha Keys");
  ImGui::PushID("alpha_keys");

  auto& alpha_keys = gradient.alpha_keys;
  auto removed_alpha_index = std::optional<std::size_t>{};

  for (auto index = std::size_t{0u}; index < alpha_keys.size(); ++index) {
    ImGui::PushID(static_cast<std::int32_t>(index));

    auto& key = alpha_keys[index];

    changed |= ImGui::SliderFloat("Time", &key.time, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Alpha", &key.alpha, 0.0f, 1.0f);

    if (ImGui::SmallButton(ICON_MDI_DELETE " Remove Alpha Key")) {
      removed_alpha_index = index;
      changed = true;
    }

    ImGui::Separator();
    ImGui::PopID();
  }

  if (removed_alpha_index) {
    for (auto i = *removed_alpha_index; i + 1u < alpha_keys.size(); ++i) {
      alpha_keys[i] = alpha_keys[i + 1u];
    }

    alpha_keys.pop_back();
  }

  ImGui::BeginDisabled(alpha_keys.size() >= sbx::assets::gradient_max_keys);

  if (ImGui::Button(ICON_MDI_PLUS " Add Alpha Key")) {
    alpha_keys.push_back(sbx::assets::gradient_alpha_key{});
    changed = true;
  }

  ImGui::EndDisabled();
  ImGui::PopID();

  ImGui::TreePop();

  return changed;
}

} // namespace editor::widgets
