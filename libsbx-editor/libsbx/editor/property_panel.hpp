// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_PROPERTY_PANEL_HPP_
#define LIBSBX_EDITOR_PROPERTY_PANEL_HPP_

#include <array>

#include <imgui.h>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/quaternion.hpp>

#include <libsbx/scenes/scene.hpp>

namespace sbx::editor {

class property_panel {

public:

  property_panel();

  auto render(const math::uuid& selected_node_id) -> void;

private:

  template<typename Component, typename Callable>
  requires (std::is_invocable_v<Callable, Component&>)
  static auto _draw_component_section(const char* title, sbx::scenes::scene& scene, sbx::scenes::node node, Callable&& callable) -> void {
    if (!scene.has_component<Component>(node)) {
      return;
    }

    const auto flags = ImGuiTreeNodeFlags{ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding};

    ImGui::PushID(title);

    const bool is_open = ImGui::TreeNodeEx("##component_header", flags, "%s", title);

    const auto header_min = ImGui::GetItemRectMin();
    const auto header_max = ImGui::GetItemRectMax();

    auto& style = ImGui::GetStyle();

    const auto header_h = header_max.y - header_min.y;
    const auto pad_x = style.FramePadding.x;
    const auto pad_y = style.FramePadding.y;

    const auto btn_h = header_h - pad_y * 2.0f;
    const auto btn_w = btn_h * 1.6f;

    const auto btn_pos = ImVec2{header_max.x - btn_w - pad_x, header_min.y + (header_h - btn_h) * 0.5f};

    ImGui::SetCursorScreenPos(btn_pos);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    if (ImGui::Button("...", ImVec2(btn_w, btn_h))) {
      ImGui::OpenPopup("##component_settings");
    }
  
    ImGui::PopStyleVar();

    // auto remove_component = false;

    if (ImGui::BeginPopup("##component_settings")) {
      if (ImGui::MenuItem("Reset")) {
        // scene.get_component<Component>(node) = Component{};
      }

      if (ImGui::MenuItem("Remove")) {
        // remove_component = true;
      }

      ImGui::EndPopup();
    }

    if (is_open) {
      ImGui::Spacing();

      auto& component = scene.get_component<Component>(node);
      std::invoke(std::forward<Callable>(callable), component);

      ImGui::Spacing();
      ImGui::TreePop();
    }

    ImGui::PopID();
  }


  static auto _draw_vec3_control(const char* label, math::vector3& v, float reset_value = 0.0f, float speed = 0.1f) -> bool;

  math::uuid _last_selected{};
  std::array<char, 256> _name_buffer{};

}; // class property_panel

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_PROPERTY_PANEL_HPP_