// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/hierarchy_panel.hpp>

#include <imgui.h>

#include <editor/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

namespace {

auto draw_node_row(editor_state& state, sbx::scenes::scene& scene, sbx::ecs::entity entity) -> void {
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

  if (state.is_node_selected(entity)) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  ImGui::PushID(static_cast<int>(entity));

  const auto is_open = ImGui::TreeNodeEx("##node_row", flags, "%s %s", ICON_MDI_CUBE_OUTLINE, tag.c_str());

  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    state.select_node(entity);
  }

  if (is_open && !relationship.children.empty()) {
    for (const auto child : relationship.children) {
      draw_node_row(state, scene, child);
    }

    ImGui::TreePop();
  }

  ImGui::PopID();
}

} // namespace

auto draw_hierarchy_panel(editor_state& state) -> void {
  ImGui::Begin(ICON_MDI_FILE_TREE " Hierarchy###hierarchy_panel");

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto has_any = false;

  for (const auto entity : scene.query<sbx::scenes::relationship>()) {
    if (scene.node_of(entity).get_component<sbx::scenes::relationship>().parent == sbx::ecs::null_entity) {
      has_any = true;
      draw_node_row(state, scene, entity);
    }
  }

  if (!has_any) {
    ImGui::TextDisabled("No nodes in the active scene.");
  }

  if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
    state.clear_selection();
  }

  ImGui::End();
}

} // namespace editor
