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
  ImGui::Begin("Inspector");

  auto& scene_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scene_module.scene();

  auto node = scene.find_node(selected_node_id);

  if (node == scenes::node::null) {
    ImGui::TextDisabled("No node selected.");
    ImGui::End();

    return;
  }

  ImGui::BeginChild("inspector_body", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

  if (selected_node_id != _last_selected) {
    _last_selected = selected_node_id;

    _name_buffer.fill('\0');

    auto& tag = scene.get_component<sbx::scenes::tag>(node);
    std::memcpy(_name_buffer.data(), tag.data(), std::min(tag.size(), _name_buffer.size() - 1));
  }

  _draw_component_section<scenes::tag>("Tag", scene, node, [this](auto& tag){
    ImGui::SetNextItemWidth(-1);

    if (ImGui::InputText("##tag_value", _name_buffer.data(), _name_buffer.size())) {
      tag = _name_buffer.data();
    }
  });

  _draw_component_section<sbx::scenes::transform>("Transform", scene, node, [this](auto& transform) {
    auto& position = transform.position();
    auto& rotation = transform.rotation();
    auto& scale = transform.scale();

    _draw_vec3_control("Position", position, 0.0f, 0.1f);

    auto euler = math::quaternion::euler_angles(rotation);

    if (_draw_vec3_control("Rotation", euler, 0.0f, 0.5f)) {
      transform.set_rotation(math::quaternion::normalized(math::quaternion{euler}));
    }

    _draw_vec3_control("Scale", scale, 1.0f, 0.1f);
  });

  ImGui::PopStyleVar(2);

  ImGui::Dummy(ImVec2(0.0f, 8.0f));
  ImGui::Separator();
  ImGui::Dummy(ImVec2(0.0f, 8.0f));

  const auto button_w = 220.0f;

  const auto avail = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - button_w) * 0.5f);

  if (ImGui::Button("Add Component", ImVec2(button_w, 0.0f))) {
    ImGui::OpenPopup("add_component_popup");
  }

  ImGui::EndChild();
  ImGui::End();
}

auto property_panel::_draw_vec3_control(const char* label, math::vector3& v, float reset_value, float speed) -> bool {
  auto has_changed = false;

  ImGui::PushID(label);

  if (ImGui::BeginTable("##vec3", 2, ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);

    ImGui::TableSetColumnIndex(1);

    const auto button_w = 25.0f;
    const auto spacing = ImGui::GetStyle().ItemSpacing.x;
    const auto avail = ImGui::GetContentRegionAvail().x;
    const auto item_w = (avail - button_w * 3.0f - spacing * 6.0f) / 3.0f;

    auto tint = [](ImVec4 c, float m) {
      c.x = (c.x * m > 1.0f) ? 1.0f : c.x * m;
      c.y = (c.y * m > 1.0f) ? 1.0f : c.y * m;
      c.z = (c.z * m > 1.0f) ? 1.0f : c.z * m;
      return c;
    };

    auto axis_color = [](const char* axis_label) -> std::array<std::pair<ImGuiCol, ImVec4>, 3u> {
      if (axis_label[0] == 'X') {
        return {{
          std::make_pair(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f }),
          std::make_pair(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f,  1.0f }),
          std::make_pair(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f })
        }};
      }

      if (axis_label[0] == 'Y') {
        return {{
          std::make_pair(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f }),
          std::make_pair(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f }),
          std::make_pair(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f })
        }};
      }

      return {{
        std::make_pair(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f }),
        std::make_pair(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f }),
        std::make_pair(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f })
      }};
    };

    auto axis = [&](const char* axis_label, float& value) {
      const auto colors = axis_color(axis_label);

      for (const auto& [id, color] : colors) {
        ImGui::PushStyleColor(id, color);
      }

      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1, 1, 1, 1});

      if (ImGui::Button(axis_label, ImVec2(button_w, 0))) {
        value = reset_value;
        has_changed = true;
      }

      ImGui::PopStyleColor(4);

      ImGui::SameLine();
      ImGui::SetNextItemWidth(item_w);

      auto lable = fmt::format("##{}", axis_label);

      has_changed |= ImGui::DragFloat(lable.c_str(), &value, speed);
    };

    axis("X", v.x());
    ImGui::SameLine();
    axis("Y", v.y());
    ImGui::SameLine();
    axis("Z", v.z());

    ImGui::EndTable();
  }

  ImGui::PopID();

  return has_changed;
}


} // namespace sbx::editor