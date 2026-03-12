// SPDX-License-Identifier: MIT
#include <libsbx/editor/hierarchy_panel.hpp>

#include <imgui.h>
#include <imnodes.h>
#include <implot.h>
// #include <ImGuizmo.h>

#include <libsbx/editor/bindings/imgui.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/relationship.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/camera.hpp>

namespace sbx::editor {

hierarchy_panel::hierarchy_panel()
: _selected_node_id{math::uuid::null()} { }

auto hierarchy_panel::render() -> void {
  ImGui::Begin("Hierarchy");

  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scene_module.scene();
  auto& graph = scene.graph();

  auto root = graph.root();

  auto& relationship = graph.get_component<scenes::relationship>(root);

  for (const auto& child : relationship.children()) {
    if (child != scenes::node::null) {
      _build_tree(child);
    }
  }

  const auto custom_backdrop_color = ImVec4{0.2f, 0.2f, 0.2f, 0.5f};

  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, custom_backdrop_color);

  _new_node_popup();
  _add_component_popup();
  _delete_node_popup();

  ImGui::PopStyleColor();

  ImGui::End();
}

auto hierarchy_panel::selected_node_id() const -> const math::uuid {
  return _selected_node_id;
}

static auto _node_or_any_child_has_camera(const scenes::node node) -> bool {
  if (node == scenes::node::null) {
    return false;
  }

  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scene_module.scene();
  auto& graph = scene.graph();

  if (graph.has_component<scenes::camera>(node)) {
    return true;
  }

  const auto& relationship = graph.get_component<sbx::scenes::relationship>(node);

  auto result = false;

  for (const auto child : relationship.children()) {
    result |= _node_or_any_child_has_camera(child);
  }

  return result;
}

auto hierarchy_panel::_context_menu() -> void {
  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scene_module.scene();
  auto& graph = scene.graph();

  auto node = graph.find_node(_selected_node_id);

  if (node == scenes::node::null) {
    if (ImGui::BeginPopupContextItem("Actions")) {
      if (ImGui::BeginMenu("Add")) {
        if (ImGui::MenuItem("New Node")) {
          _open_popups.set(popup::new_node);
        }

        ImGui::EndMenu();
      }

      ImGui::EndPopup();
    }

    return;
  }

  if (ImGui::BeginPopupContextItem("Actions")) {
    if (ImGui::BeginMenu("Add")) {
      if (ImGui::MenuItem("New Node")) {
        _open_popups.set(popup::new_node);
      }

      if (ImGui::MenuItem("Component")) {
        _open_popups.set(popup::add_component);
      }

      if (ImGui::MenuItem("Test")) {
        
      }

      ImGui::EndMenu();
    }

    if (_selected_node_id != graph.get_component<sbx::scenes::id>(graph.root()) && !_node_or_any_child_has_camera(node)) {
      if (ImGui::MenuItem("Delete")) {
        _open_popups.set(popup::delete_node);
      }
    }

    ImGui::EndPopup();
  }
}

auto hierarchy_panel::_new_node_popup() -> void {
  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scene_module.scene();
  auto& graph = scene.graph();

  if (_open_popups.has(popup::new_node)) {
    _open_popups.clear(popup::new_node);
    ImGui::OpenPopup("New Node");
  }

  ImGui::SetNextWindowSizeConstraints(ImVec2{200, 120}, ImVec2{FLT_MAX, FLT_MAX});

  if (ImGui::BeginPopupModal("New Node", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Name");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputText("##NodeNameInput", _new_name_buffer.data(), _new_name_buffer.size());

    const auto name = std::string{_new_name_buffer.data(), std::strlen(_new_name_buffer.data())};

    const auto button_width = 75.0f;
    const auto padding = 10.0f;
    const auto available_width = ImGui::GetContentRegionAvail().x;
    const auto total_width = (button_width * 2.0f) + padding;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available_width - total_width);

    const auto footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMax().y - footer_height);

    if (ImGui::Button("Cancel", ImVec2{button_width, 0})) {
      _new_name_buffer.fill('\0');
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (name.empty()) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Create", ImVec2{button_width, 0})) {
      if (auto node = graph.find_node(_selected_node_id); node != scenes::node::null) {
        graph.create_child_node(node, name);
      } else {
        graph.create_child_node(graph.root(), name);
      }

      _new_name_buffer.fill('\0');
      ImGui::CloseCurrentPopup();
    }

    if (name.empty()) {
      ImGui::EndDisabled();
    }

    ImGui::EndPopup();
  }
}

auto hierarchy_panel::_add_component_popup() -> void {
  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scene_module.scene();

  if (_open_popups.has(popup::add_component)) {
    _open_popups.clear(popup::add_component);
    ImGui::OpenPopup("Add Component");
  }

  ImGui::SetNextWindowSizeConstraints(ImVec2{200, 120}, ImVec2{FLT_MAX, FLT_MAX});

  if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Not implemented");

    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

auto hierarchy_panel::_delete_node_popup() -> void {
  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scene_module.scene();
  auto& graph = scene.graph();

  if (_open_popups.has(popup::delete_node)) {
    _open_popups.clear(popup::delete_node);
    ImGui::OpenPopup("Delete Node");
  }

  ImGui::SetNextWindowSizeConstraints(ImVec2{200, 120}, ImVec2{FLT_MAX, FLT_MAX});

  if (ImGui::BeginPopupModal("Delete Node", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    const auto button_width = 75.0f;
    const auto padding = 10.0f;
    const auto available_width = ImGui::GetContentRegionAvail().x;
    const auto total_width = (button_width * 2.0f) + padding;

    if (auto node = graph.find_node(_selected_node_id); node != scenes::node::null) {
      ImGui::Text("Do you want to delete '%s'", graph.get_component<sbx::scenes::tag>(node).c_str());

      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available_width - total_width);

      const auto footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
      ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMax().y - footer_height);

      if (ImGui::Button("Cancel", ImVec2{button_width, 0})) {
        ImGui::CloseCurrentPopup();
      }

      ImGui::SameLine();

      if (ImGui::Button("Delete", ImVec2{button_width, 0})) {
        graph.destroy_node(node);
        _selected_node_id = sbx::math::uuid::null();
        ImGui::CloseCurrentPopup();
      }
    } else {
      ImGui::Text("No node selected (This can be considered a programmer error)");

      if (ImGui::Button("Cancel", ImVec2{button_width, 0})) {
        ImGui::CloseCurrentPopup();
      }
    }

    ImGui::EndPopup();
  }
}

auto hierarchy_panel::_build_tree(const sbx::scenes::node node) -> void {
  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scene_module.scene();
  auto& graph = scene.graph();

  if (!graph.is_valid(node)) {
    return;
  }

  const auto& relationship = graph.get_component<sbx::scenes::relationship>(node);

  auto flag = ImGuiTreeNodeFlags{ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen};

  if (relationship.children().empty()) {
    flag |= ImGuiTreeNodeFlags_Leaf;
  }

  if (graph.get_component<sbx::scenes::id>(node) == _selected_node_id) {
    flag |= ImGuiTreeNodeFlags_Selected;
  }

  const auto& tag = graph.get_component<sbx::scenes::tag>(node);
  const auto& id = graph.get_component<sbx::scenes::id>(node);
  const auto* ptr_id = reinterpret_cast<const void*>(static_cast<std::intptr_t>(id.value()));

  ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{2.0f, 2.0f});

  if (ImGui::TreeNodeEx(ptr_id, flag, "%s", tag.c_str())) {
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      _selected_node_id = graph.get_component<sbx::scenes::id>(node);

      utility::logger<"editor">::debug("Selected node id {}", _selected_node_id);
    }

    _context_menu();

    for (const auto& child : relationship.children()) {
      if (child != scenes::node::null) {
        _build_tree(child);
      }
    }

    ImGui::TreePop();
  }

  ImGui::PopStyleVar(2);
}

} // namespace sbx::editor