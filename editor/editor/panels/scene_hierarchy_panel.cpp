// SPDX-License-Identifier: MIT
#include <editor/panels/scene_hierarchy_panel.hpp>

#include <imgui.h>

#include <fmt/format.h>

#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>

namespace editor {

auto scene_hierarchy_panel::draw(sbx::scenes::scene& scene) -> void {
  ImGui::Begin("Scene Hierarchy");

  auto& graph = scene.graph();
  auto root = graph.root();

  const auto& root_relationship = graph.get_component<sbx::scenes::relationship>(root);

  for (auto child : root_relationship.children()) {
    if (child == sbx::scenes::node::null) {
      continue;
    }

    _draw_node(graph, child);
  }

  if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
    _selected_node = sbx::scenes::node::null;
  }

  if (ImGui::BeginPopupContextWindow("##hierarchy_context", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
    if (ImGui::MenuItem("Create Node")) {
      graph.create_node("New Node");
    }

    ImGui::EndPopup();
  }

  ImGui::End();
}

auto scene_hierarchy_panel::_draw_node(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  const auto& tag = graph.get_component<sbx::scenes::tag>(node);
  const auto& relationship = graph.get_component<sbx::scenes::relationship>(node);

  auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

  if (_selected_node == node) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  if (relationship.children().empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  auto label = fmt::format("{}##{}", tag.c_str(), static_cast<std::uint32_t>(node));
  auto is_open = ImGui::TreeNodeEx(label.c_str(), flags);

  if (ImGui::IsItemClicked()) {
    _selected_node = node;
  }

  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Create Child")) {
      graph.create_child_node(node, "New Node");
    }

    if (ImGui::MenuItem("Delete")) {
      if (_selected_node == node) {
        _selected_node = sbx::scenes::node::null;
      }

      graph.destroy_node(node);

      ImGui::EndPopup();

      if (is_open && !relationship.children().empty()) {
        ImGui::TreePop();
      }

      return;
    }

    ImGui::EndPopup();
  }

  if (is_open && !relationship.children().empty()) {
    for (auto child : relationship.children()) {
      if (child == sbx::scenes::node::null) {
        continue;
      }

      _draw_node(graph, child);
    }

    ImGui::TreePop();
  }
}

} // namespace editor
