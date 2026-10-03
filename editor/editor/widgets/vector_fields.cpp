// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/widgets/vector_fields.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include <imgui.h>
#include <imgui_internal.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/editor_preferences.hpp>

namespace editor {

auto draw_trailing_label(const char* label) -> void {
  const auto* label_end = ImGui::FindRenderedTextEnd(label);

  if (label_end != label) {
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::TextUnformatted(label, label_end);
  }
}

template<std::size_t Size>
auto draw_vector_control(const char* label, std::array<std::float_t, Size>& values, const std::array<std::float_t, Size>& reset_values, std::float_t speed, std::float_t min, std::float_t max) -> vector_edit_result {
  auto result = vector_edit_result{};

  ImGui::BeginGroup();
  ImGui::PushID(label);
  ImGui::PushMultiItemsWidths(static_cast<std::int32_t>(Size), ImGui::CalcItemWidth());

  for (auto axis = std::size_t{0u}; axis < Size; ++axis) {
    ImGui::PushID(static_cast<std::int32_t>(axis));

    if (axis != 0u) {
      ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    }

    ImGui::SetNextItemColorMarker(axis_colors()[axis]);
    result.changed |= ImGui::DragFloat("", &values[axis], speed, min, max, "%.3f");

    ImGui::PopID();
    ImGui::PopItemWidth();
  }

  ImGui::PopID();
  draw_trailing_label(label);
  ImGui::EndGroup();

  result.started = ImGui::IsItemActivated();
  result.committed = ImGui::IsItemDeactivatedAfterEdit();
  result.ended = ImGui::IsItemDeactivated();

  ImGui::PushID(label);

  if (ImGui::BeginPopupContextItem("##reset")) {
    if (ImGui::MenuItem("Reset")) {
      values = reset_values;
      result.changed = true;
      result.started = true;
      result.committed = true;
      result.ended = true;
    }

    ImGui::EndPopup();
  }

  ImGui::PopID();

  return result;
}

auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, std::float_t reset_value, std::float_t speed, std::float_t min, std::float_t max) -> vector3_edit_result {
  return draw_vector_control<3u>(label, values, {reset_value, reset_value, reset_value}, speed, min, max);
}

auto draw_vector3_control(const char* label, std::array<std::float_t, 3u>& values, const std::array<std::float_t, 3u>& reset_values, std::float_t speed, std::float_t min, std::float_t max) -> vector3_edit_result {
  return draw_vector_control<3u>(label, values, reset_values, speed, min, max);
}

auto draw_vector2_control(const char* label, std::array<std::float_t, 2u>& values, std::float_t reset_value, std::float_t speed, std::float_t min, std::float_t max) -> vector2_edit_result {
  return draw_vector_control<2u>(label, values, {reset_value, reset_value}, speed, min, max);
}

auto draw_vector2_control(const char* label, std::array<std::float_t, 2u>& values, const std::array<std::float_t, 2u>& reset_values, std::float_t speed, std::float_t min, std::float_t max) -> vector2_edit_result {
  return draw_vector_control<2u>(label, values, reset_values, speed, min, max);
}

auto draw_color_edit(const char* label, std::float_t* values, std::int32_t components) -> bool {
  const auto& style = ImGui::GetStyle();
  auto changed = false;

  ImGui::BeginGroup();
  ImGui::PushID(label);
  ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth() - ImGui::GetFrameHeight() - style.ItemInnerSpacing.x);

  for (auto channel = std::int32_t{0}; channel < components; ++channel) {
    ImGui::PushID(channel);

    if (channel != 0) {
      ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    }

    auto value = static_cast<std::int32_t>(std::round(std::clamp(values[channel], 0.0f, 1.0f) * 255.0f));

    ImGui::SetNextItemColorMarker(axis_colors()[static_cast<std::size_t>(channel)]);

    if (ImGui::DragInt("", &value, 1.0f, 0, 255)) {
      values[channel] = static_cast<std::float_t>(value) / 255.0f;
      changed = true;
    }

    ImGui::PopID();
    ImGui::PopItemWidth();
  }

  ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);

  const auto swatch_flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
  changed |= (components == 4) ? ImGui::ColorEdit4("##swatch", values, swatch_flags) : ImGui::ColorEdit3("##swatch", values, swatch_flags);

  ImGui::PopID();
  draw_trailing_label(label);
  ImGui::EndGroup();

  return changed;
}

auto draw_color_field(const char* label, sbx::math::color& color) -> bool {
  auto value = std::array<std::float_t, 4u>{color.r(), color.g(), color.b(), color.a()};

  if (!draw_color_edit(label, value.data(), 4)) {
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

} // namespace editor
