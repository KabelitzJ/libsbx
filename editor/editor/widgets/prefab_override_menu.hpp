// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_WIDGETS_PREFAB_OVERRIDE_MENU_HPP_
#define EDITOR_WIDGETS_PREFAB_OVERRIDE_MENU_HPP_

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <editor/widgets/property_row.hpp>

namespace editor {

/**
 * @brief The node's value override on a component, if its prefab instance diverges there.
 *
 * @param target The scene.
 * @param node The node.
 * @param component_key The component's serialized type key.
 *
 * @return The override, if any.
 */
inline auto find_value_override(sbx::scenes::scene& target, const sbx::scenes::node& node, std::string_view component_key) -> std::optional<sbx::scenes::prefab_override> {
  if (component_key.empty()) {
    return std::nullopt;
  }

  for (auto& override_entry : sbx::scenes::scene_serializer::prefab_overrides_of(target, node)) {
    if (override_entry.component_key == component_key && override_entry.kind == sbx::scenes::prefab_override_kind::component_value) {
      return override_entry;
    }
  }

  return std::nullopt;
}

/**
 * @brief Turns a serialized key into a menu label, e.g. "fov_degrees" -> "Fov Degrees".
 *
 * @param field The field key.
 *
 * @return The label.
 */
inline auto pretty_field_name(std::string_view field) -> std::string {
  auto result = std::string{field};
  auto at_word_start = true;

  for (auto& character : result) {
    if (character == '_') {
      character = ' ';
      at_word_start = true;
    } else if (at_word_start) {
      character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
      at_word_start = false;
    }
  }

  return result;
}

/**
 * @brief Whether an override covers @p field; one without a field list covers the whole component.
 *
 * @param override_entry The override.
 * @param field The field key.
 *
 * @return True if covered.
 */
inline auto overrides_field(const std::optional<sbx::scenes::prefab_override>& override_entry, std::string_view field) -> bool {
  return override_entry && (override_entry->fields.empty() || std::ranges::find(override_entry->fields, field) != override_entry->fields.end());
}

/**
 * @brief Marks the last drawn component header as overridden, with a tooltip naming the fields. Not undoable, since apply/revert also rewrite the prefab asset.
 *
 * @param override_entry The override, if any; no-op without one.
 */
inline auto draw_override_header_marker(const std::optional<sbx::scenes::prefab_override>& override_entry) -> void {
  if (!override_entry) {
    return;
  }

  draw_override_marker();

  if (ImGui::IsItemHovered()) {
    auto text = std::string{"Overrides the prefab"};

    if (!override_entry->fields.empty()) {
      text += ":";

      for (const auto& field : override_entry->fields) {
        text += "\n  " + pretty_field_name(field);
      }
    }

    ImGui::SetTooltip("%s", text.c_str());
  }
}

inline auto draw_override_menu_items(sbx::scenes::scene& target, sbx::scenes::node& node, std::string_view component_key, const std::optional<sbx::scenes::prefab_override>& override_entry) -> void {
  if (!override_entry) {
    return;
  }

  ImGui::Separator();

  if (ImGui::BeginMenu(ICON_MDI_UNDO " Revert to Prefab")) {
    for (const auto& field : override_entry->fields) {
      if (ImGui::MenuItem(pretty_field_name(field).c_str())) {
        sbx::scenes::scene_serializer::revert_prefab_override_field(target, node, component_key, field);
      }
    }

    if (!override_entry->fields.empty()) {
      ImGui::Separator();
    }

    if (ImGui::MenuItem("All")) {
      sbx::scenes::scene_serializer::revert_prefab_override(target, node, component_key);
    }

    ImGui::EndMenu();
  }

  if (ImGui::MenuItem(ICON_MDI_SOURCE_MERGE " Apply to Prefab")) {
    sbx::scenes::scene_serializer::apply_prefab_override(target, node, component_key);
  }
}

} // namespace editor

#endif // EDITOR_WIDGETS_PREFAB_OVERRIDE_MENU_HPP_
