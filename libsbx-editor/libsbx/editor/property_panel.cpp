// SPDX-License-Identifier: MIT
#include <libsbx/editor/property_panel.hpp>

#include <imgui.h>
#include <imnodes.h>
#include <implot.h>
// #include <ImGuizmo.h>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/transform.hpp>

namespace sbx::editor {

property_panel::property_panel() {

}

auto property_panel::render(const math::uuid& selected_node_id) -> void {
  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& scene = scene_module.scene();

  if (auto node = scene.find_node(selected_node_id); node != scenes::node::null) {
    if (ImGui::TreeNodeEx("Tag", ImGuiTreeNodeFlags_DefaultOpen)) {
      _name_buffer.fill('\0');

      auto& tag = scene.get_component<sbx::scenes::tag>(node);

      std::memcpy(_name_buffer.data(), tag.data(), std::min(tag.size(), _name_buffer.size()));

      if (ImGui::InputText("##label", _name_buffer.data(), _name_buffer.size())) {
        tag = _name_buffer.data();
      }

      ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto& transform = scene.get_component<sbx::scenes::transform>(node);

      ImGui::Text("Position");

      auto& position = transform.position();

      ImGui::Text("x");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100); // Adjust slider width
      ImGui::DragFloat("##XPosition", &position.x(), 0.1f);
      ImGui::SameLine();

      ImGui::Text("y");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::DragFloat("##YPosition", &position.y(), 0.1f);
      ImGui::SameLine();

      ImGui::Text("z");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::DragFloat("##ZPosition", &position.z(), 0.1f);

      ImGui::Text("Scale");

      auto& scale = transform.scale();

      ImGui::Text("x");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::DragFloat("##XScale", &scale.x(), 0.1f, 0.1f, 10.0f);
      ImGui::SameLine();

      ImGui::Text("y");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::DragFloat("##YScale", &scale.y(), 0.1f, 0.1f, 10.0f);
      ImGui::SameLine();

      ImGui::Text("z");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(100);
      ImGui::DragFloat("##ZScale", &scale.z(), 0.1f, 0.1f, 10.0f);
      ImGui::SameLine();

      ImGui::TreePop();
    }
  }
}

} // namespace sbx::editor