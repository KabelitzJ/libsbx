// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/hierarchy_panel.hpp>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {


// Matches the same glyphs Properties uses as each component's section-header icon, so a node's
// row here and its component sections there always agree. Checked in a fixed priority order —
// a node with more than one of these just shows the first match.
auto icon_for(const sbx::scenes::node& node) -> const char* {
  if (node.has_component<sbx::scenes::camera>()) return ICON_MDI_CAMERA_OUTLINE;
  if (node.has_component<sbx::scenes::directional_light>()) return ICON_MDI_WHITE_BALANCE_SUNNY;
  if (node.has_component<sbx::scenes::point_light>()) return ICON_MDI_LIGHTBULB_OUTLINE;
  if (node.has_component<sbx::scenes::spot_light>()) return ICON_MDI_FLASHLIGHT;
  if (node.has_component<sbx::scenes::skybox>()) return ICON_MDI_EARTH;
  if (node.has_component<sbx::scenes::mesh_renderer>()) return ICON_MDI_CUBE_OUTLINE;
  return ICON_MDI_AXIS_ARROW; // plain transform/group node — no renderable/functional component
}


auto hierarchy_panel::_draw_node_row(editor_state& state, sbx::scenes::scene& scene, sbx::ecs::entity entity) -> void {
  auto node = scene.node_of(entity);

  if (!node.is_valid()) {
    return;
  }

  const auto& relationship = node.get_component<sbx::scenes::relationship>();
  const auto& tag = node.name();

  auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

  if (relationship.children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  if (state.is_node_selected(node)) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  ImGui::PushID(static_cast<int>(entity));

  const auto is_open = ImGui::TreeNodeEx("##node_row", flags, "%s %s", icon_for(node), tag.c_str());

  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    state.select_node(node);
  }

  if (ImGui::BeginPopupContextItem("##node_context")) {
    if (ImGui::MenuItem(ICON_MDI_PLUS " Add Child")) {
      // Deferred — see _pending_add_child_parent_id's declaration for why this can't happen here.
      _pending_add_child_parent_id = node.id();
    }

    if (ImGui::MenuItem(ICON_MDI_DELETE " Delete Node")) {
      _pending_delete_id = node.id();
    }

    ImGui::EndPopup();
  }

  if (is_open && !relationship.children.empty()) {
    for (const auto child : relationship.children) {
      _draw_node_row(state, scene, child);
    }

    ImGui::TreePop();
  }

  ImGui::PopID();
}

auto hierarchy_panel::draw(editor_state& state) -> void {
  ImGui::Begin(ICON_MDI_FILE_TREE " Hierarchy###hierarchy_panel");

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto has_any = false;

  for (const auto entity : scene.query<sbx::scenes::relationship>()) {
    if (scene.node_of(entity).get_component<sbx::scenes::relationship>().parent == sbx::ecs::null_entity) {
      has_any = true;
      _draw_node_row(state, scene, entity);
    }
  }

  if (!has_any) {
    ImGui::TextDisabled("No nodes in the active scene.");
  }

  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
    state.clear_selection();
  }

  // Right-click on empty space (below/between rows, never over a row — that's each row's own
  // ##node_context popup) adds a new top-level node.
  if (ImGui::BeginPopupContextWindow("##hierarchy_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
    if (ImGui::MenuItem(ICON_MDI_PLUS " Add Node")) {
      state.select_node(scene.create_node());
    }

    ImGui::EndPopup();
  }

  if (ImGui::IsWindowHovered() && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
    if (auto selected = state.selected_node(scene); selected.is_valid()) {
      _pending_delete_id = selected.id();
    }
  }

  if (_pending_delete_id != sbx::math::uuid::nil()) {
    if (auto target = scene.find(_pending_delete_id); target.is_valid()) {
      scene.destroy_node(target);
    }

    _pending_delete_id = sbx::math::uuid::nil();
  }

  if (_pending_add_child_parent_id != sbx::math::uuid::nil()) {
    if (auto parent = scene.find(_pending_add_child_parent_id); parent.is_valid()) {
      auto child = scene.create_node();
      child.set_parent(parent);
      state.select_node(child);
    }

    _pending_add_child_parent_id = sbx::math::uuid::nil();
  }

  ImGui::End();
}

} // namespace editor
