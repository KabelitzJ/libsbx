// SPDX-License-Identifier: MIT
#include <editor/panels/inspector_panel.hpp>

#include <imgui.h>

#include <fmt/format.h>

#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/skinned_mesh.hpp>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/angle.hpp>

namespace editor {

auto inspector_panel::draw(sbx::scenes::scene& scene, sbx::scenes::node selected_node) -> void {
  ImGui::Begin("Inspector");

  if (selected_node == sbx::scenes::node::null) {
    ImGui::TextDisabled("No node selected");
    ImGui::End();
    return;
  }

  auto& graph = scene.graph();

  if (!graph.is_valid(selected_node)) {
    ImGui::TextDisabled("Invalid node");
    ImGui::End();
    return;
  }

  // Tag (name)
  auto& tag = graph.get_component<sbx::scenes::tag>(selected_node);
  _tag_buffer = tag.str();
  _tag_buffer.resize(256);

  if (ImGui::InputText("##tag", _tag_buffer.data(), _tag_buffer.capacity(), ImGuiInputTextFlags_EnterReturnsTrue)) {
    auto new_tag = std::string{_tag_buffer.c_str()};

    if (!new_tag.empty()) {
      graph.get_component<sbx::scenes::tag>(selected_node) = sbx::scenes::tag{new_tag};
    }
  }

  ImGui::Separator();

  // Transform
  _draw_transform(graph, selected_node);

  // Optional components
  if (graph.has_component<sbx::scenes::directional_light>(selected_node)) {
    _draw_directional_light(graph, selected_node);
  }

  if (graph.has_component<sbx::scenes::point_light>(selected_node)) {
    _draw_point_light(graph, selected_node);
  }

  if (graph.has_component<sbx::scenes::camera>(selected_node)) {
    _draw_camera(graph, selected_node);
  }

  if (graph.has_component<sbx::scenes::static_mesh>(selected_node)) {
    _draw_static_mesh(graph, selected_node);
  }

  ImGui::End();
}

auto inspector_panel::_draw_transform(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& transform = graph.get_component<sbx::scenes::transform>(node);

  auto position = transform.position();
  if (_draw_vec3_control("Position", position)) {
    transform.set_position(position);
  }

  auto euler = sbx::math::quaternion::euler_angles(transform.rotation());
  if (_draw_vec3_control("Rotation", euler)) {
    transform.set_rotation(sbx::math::quaternion{euler});
  }

  auto scale = transform.scale();
  if (_draw_vec3_control("Scale", scale, 1.0f)) {
    transform.set_scale(scale);
  }
}

auto inspector_panel::_draw_directional_light(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& light = graph.get_component<sbx::scenes::directional_light>(node);

  auto direction = light.direction();
  if (_draw_vec3_control("Direction", direction)) {
    light.set_direction(direction);
  }

  auto color = std::array<std::float_t, 3>{light.color().r(), light.color().g(), light.color().b()};

  if (ImGui::ColorEdit3("Color", color.data())) {
    light.color().r() = color[0];
    light.color().g() = color[1];
    light.color().b() = color[2];
  }
}

auto inspector_panel::_draw_point_light(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& light = graph.get_component<sbx::scenes::point_light>(node);

  auto color = std::array<std::float_t, 3>{light.color().r(), light.color().g(), light.color().b()};

  if (ImGui::ColorEdit3("Color##point_light", color.data())) {
    // point_light color is const; display only for now
  }

  ImGui::Text("Radius: %.2f", light.radius());
}

auto inspector_panel::_draw_camera(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& cam = graph.get_component<sbx::scenes::camera>(node);

  auto fov = cam.field_of_view().to_degrees().value();

  if (ImGui::DragFloat("Field of View", &fov, 0.5f, 1.0f, 179.0f, "%.1f deg")) {
    cam.set_field_of_view(sbx::math::angle{sbx::math::degree{fov}});
  }

  ImGui::Text("Near: %.3f", cam.near_plane());
  ImGui::Text("Far: %.1f", cam.far_plane());
}

auto inspector_panel::_draw_static_mesh(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  const auto& mesh = graph.get_component<sbx::scenes::static_mesh>(node);

  ImGui::Text("Mesh: %s", fmt::format("{}", mesh.mesh_id()).c_str());
  ImGui::Text("Submeshes: %zu", mesh.submeshes().size());

  for (auto i = 0u; i < mesh.submeshes().size(); ++i) {
    ImGui::Text("  [%u] Material: %s", i, fmt::format("{}", mesh.submeshes()[i].material).c_str());
  }
}

auto inspector_panel::_draw_vec3_control(const std::string& label, sbx::math::vector3& values, float reset_value) -> bool {
  auto changed = false;

  ImGui::PushID(label.c_str());

  ImGui::Columns(2);
  ImGui::SetColumnWidth(0, 80.0f);
  ImGui::Text("%s", label.c_str());
  ImGui::NextColumn();

  auto region = ImGui::GetContentRegionAvail().x;
  auto spacing = 4.0f;
  auto line_height = ImGui::GetFrameHeight();
  auto button_width = line_height;
  auto item_width = std::max(40.0f, (region - button_width * 3.0f - spacing * 2.0f) / 3.0f);
  auto button_size = ImVec2{button_width, line_height};

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{2.0f, 0.0f});

  // X
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.7f, 0.15f, 0.15f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.85f, 0.2f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.7f, 0.15f, 0.15f, 1.0f});

  if (ImGui::Button("X", button_size)) {
    values.x() = reset_value;
    changed = true;
  }

  ImGui::PopStyleColor(3);
  ImGui::SameLine();

  auto x = values.x();

  ImGui::PushItemWidth(item_width);

  if (ImGui::DragFloat("##x", &x, 0.05f, 0.0f, 0.0f, "%.3f")) {
    values.x() = x;
    changed = true;
  }

  ImGui::PopItemWidth();
  ImGui::SameLine(0.0f, spacing);

  // Y
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.15f, 0.6f, 0.15f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.75f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.15f, 0.6f, 0.15f, 1.0f});

  if (ImGui::Button("Y", button_size)) {
    values.y() = reset_value;
    changed = true;
  }

  ImGui::PopStyleColor(3);
  ImGui::SameLine();

  auto y = values.y();

  ImGui::PushItemWidth(item_width);

  if (ImGui::DragFloat("##y", &y, 0.05f, 0.0f, 0.0f, "%.3f")) {
    values.y() = y;
    changed = true;
  }

  ImGui::PopItemWidth();
  ImGui::SameLine(0.0f, spacing);

  // Z
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.15f, 0.15f, 0.7f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.2f, 0.85f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.15f, 0.15f, 0.7f, 1.0f});

  if (ImGui::Button("Z", button_size)) {
    values.z() = reset_value;
    changed = true;
  }

  ImGui::PopStyleColor(3);
  ImGui::SameLine();

  auto z = values.z();

  ImGui::PushItemWidth(item_width);

  if (ImGui::DragFloat("##z", &z, 0.05f, 0.0f, 0.0f, "%.3f")) {
    values.z() = z;
    changed = true;
  }

  ImGui::PopItemWidth();

  ImGui::PopStyleVar();
  ImGui::Columns(1);

  ImGui::PopID();

  return changed;
}

} // namespace editor
