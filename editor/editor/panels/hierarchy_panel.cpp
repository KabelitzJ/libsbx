// SPDX-License-Identifier: MIT
#include <editor/panels/hierarchy_panel.hpp>

#include <fmt/format.h>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>
#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/camera.hpp>

#include <editor/bindings/imgui.hpp>

namespace editor {

auto hierarchy_panel::draw() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  if (!scenes_module.has_active_scene()) {
    return;
  }

  auto& scene = scenes_module.active_scene();

  ImGui::Begin(ICON_MDI_FILE_TREE " Hierarchy###hierarchy_panel");

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
    if (ImGui::MenuItem("New Node")) {
      _create_parent = root;
      _open_create_popup = true;
    }

    ImGui::EndPopup();
  }

  _draw_create_node_popup(graph);

  ImGui::End();
}

auto hierarchy_panel::_draw_node(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  const auto& tag = graph.get_component<sbx::scenes::tag>(node);
  const auto& relationship = graph.get_component<sbx::scenes::relationship>(node);

  auto is_leaf = relationship.children().empty();

  auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

  if (_selected_node == node) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  if (is_leaf) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  const auto is_camera = graph.has_component<sbx::scenes::camera>(node);
  const auto is_light = graph.has_component<sbx::scenes::directional_light>(node) || graph.has_component<sbx::scenes::point_light>(node);

  const auto icon = is_camera ? ICON_MDI_CAMERA : (is_light ? ICON_MDI_LIGHTBULB : ICON_MDI_CUBE_OUTLINE);

  auto label = fmt::format("{} {}##{}", icon, tag.c_str(), static_cast<std::uint32_t>(node));
  auto is_open = ImGui::TreeNodeEx(label.c_str(), flags);

  if (ImGui::IsItemClicked()) {
    _selected_node = node;
  }

  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("New Node")) {
      _create_parent = node;
      _open_create_popup = true;
    }

    if (!is_camera && ImGui::MenuItem("Delete")) {
      if (_selected_node == node) {
        _selected_node = sbx::scenes::node::null;
      }

      graph.destroy_node(node);

      _selected_node = sbx::scenes::node::null;

      ImGui::EndPopup();

      if (is_open && !is_leaf) {
        ImGui::TreePop();
      }

      return;
    }

    ImGui::EndPopup();
  }

  if (is_open && !is_leaf) {
    for (auto child : relationship.children()) {
      if (child == sbx::scenes::node::null) {
        continue;
      }

      _draw_node(graph, child);
    }

    ImGui::TreePop();
  }
}

auto hierarchy_panel::_draw_create_node_popup(sbx::scenes::scene_graph& graph) -> void {
  if (_open_create_popup) {
    ImGui::OpenPopup("##create_node");
    _name_buffer.fill('\0');
    _open_create_popup = false;
  }

  auto center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});

  if (ImGui::BeginPopupModal("##create_node", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
    ImGui::Text("Node Name");
    ImGui::Separator();

    auto confirm = ImGui::InputText("##name", _name_buffer.data(), _name_buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);

    if (ImGui::IsWindowAppearing()) {
      ImGui::SetKeyboardFocusHere(-1);
    }

    auto valid = _name_buffer[0] != '\0';

    auto button_width = ImGui::CalcTextSize("Cancel").x + ImGui::CalcTextSize("Create").x + ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ItemSpacing.x;

    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - button_width + ImGui::GetStyle().WindowPadding.x);

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (!valid) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Create") || (confirm && valid)) {
      auto name = std::string{_name_buffer.data()};

      _selected_node = graph.create_child_node(_create_parent, name);

      ImGui::CloseCurrentPopup();
    }

    if (!valid) {
      ImGui::EndDisabled();
    }

    ImGui::EndPopup();
  }
}

} // namespace editor
