// SPDX-License-Identifier: MIT
#include <editor/panels/inspector_panel.hpp>

#include <fmt/format.h>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/skinned_mesh.hpp>

#include <libsbx/models/material.hpp>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/angle.hpp>

#include <editor/bindings/imgui.hpp>

#include <editor/panels/asset_browser_panel.hpp>

#include <editor/widgets/controls.hpp>

namespace editor {

auto inspector_panel::draw(const sbx::scenes::node selected_node) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  if (!scenes_module.has_active_scene()) {
    return;
  }

  auto& scene = scenes_module.active_scene();

  ImGui::Begin(ICON_MDI_INFORMATION " Inspector##inspector_panel");

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

  _draw_tag(graph, selected_node);
  _draw_transform(graph, selected_node);

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

auto inspector_panel::_draw_tag(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_TAG " Tag###tag", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& tag = graph.get_component<sbx::scenes::tag>(node);
  _tag_buffer = tag.str();
  _tag_buffer.resize(256);

  if (ImGui::InputText("##tag", _tag_buffer.data(), _tag_buffer.capacity(), ImGuiInputTextFlags_EnterReturnsTrue)) {
    auto new_tag = std::string{_tag_buffer.c_str()};

    if (!new_tag.empty()) {
      graph.get_component<sbx::scenes::tag>(node) = sbx::scenes::tag{new_tag};
    }
  }
}

auto inspector_panel::_draw_transform(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_AXIS_ARROW " Transform###transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& transform = graph.get_component<sbx::scenes::transform>(node);

  auto position = transform.position();

  if (controls::vector3("Position", position)) {
    transform.set_position(position);
  }

  auto euler = sbx::math::quaternion::euler_angles(transform.rotation());

  if (controls::vector3("Rotation", euler)) {
    transform.set_rotation(sbx::math::quaternion{euler});
  }

  auto scale = transform.scale();

  if (controls::vector3("Scale", scale, 1.0f)) {
    transform.set_scale(scale);
  }
}

auto inspector_panel::_draw_directional_light(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_LIGHTBULB " Directional Light###directional_light", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& light = graph.get_component<sbx::scenes::directional_light>(node);

  auto direction = light.direction();

  if (controls::vector3("Direction", direction)) {
    light.set_direction(direction);
  }

  controls::color3("Color", light.color());
}

auto inspector_panel::_draw_point_light(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_LIGHTBULB " Point Light###point_light", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& light = graph.get_component<sbx::scenes::point_light>(node);

  auto color_copy = light.color();
  controls::color3("Color##point_light", color_copy);

  controls::labeled_text("Radius", "%.2f", light.radius());
}

auto inspector_panel::_draw_camera(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_CAMERA " Camera###camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& cam = graph.get_component<sbx::scenes::camera>(node);

  auto fov = cam.field_of_view().to_degrees().value();

  if (ImGui::DragFloat("Field of View", &fov, 0.5f, 1.0f, 179.0f, "%.1f deg")) {
    cam.set_field_of_view(sbx::math::angle{sbx::math::degree{fov}});
  }

  controls::labeled_text("Near", "%.3f", cam.near_plane());
  controls::labeled_text("Far", "%.1f", cam.far_plane());
}

auto inspector_panel::_draw_static_mesh(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_VECTOR_POLYGON " Static Mesh###static_mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  const auto& mesh = graph.get_component<sbx::scenes::static_mesh>(node);

  controls::labeled_text("Mesh", "%s", fmt::format("{}", mesh.mesh_id()).c_str());
  controls::labeled_text("Submeshes", "%zu", mesh.submeshes().size());

  ImGui::Separator();

  for (auto i = 0u; i < mesh.submeshes().size(); ++i) {
    const auto& submesh = mesh.submeshes()[i];

    ImGui::PushID(static_cast<std::int32_t>(i));

    const auto label = fmt::format(ICON_MDI_TEXTURE_BOX " Submesh {} - Material###submesh_material_{}", i, i);

    if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      _draw_material(submesh.material, i);
      ImGui::TreePop();
    }

    ImGui::PopID();
  }
}

auto inspector_panel::_draw_material(const sbx::math::uuid& material_id, std::uint32_t submesh_index) -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  const auto& asset_registry = scenes_module.asset_registry();

  auto& material = assets_module.get_asset<sbx::models::material>(material_id);

  ImGui::PushID(static_cast<std::int32_t>(submesh_index));

  const auto& metadata = asset_registry.material_metadata(material_id);

  controls::labeled_text("Name", "%s", metadata.name.c_str());
  controls::labeled_text("UUID", "%s", fmt::format("{}", material_id).c_str());

  ImGui::Spacing();

  controls::color4("Base Color", material.base_color);

  ImGui::DragFloat("Metallic", &material.metallic_factor, 0.005f, 0.0f, 1.0f, "%.3f");
  ImGui::DragFloat("Roughness", &material.roughness_factor, 0.005f, 0.0f, 1.0f, "%.3f");
  ImGui::DragFloat("Specular", &material.specular_factor, 0.005f, 0.0f, 1.0f, "%.3f");
  ImGui::DragFloat("Occlusion", &material.occlusion_strength, 0.005f, 0.0f, 1.0f, "%.3f");
  ImGui::DragFloat("Normal Scale", &material.normal_scale, 0.01f, 0.0f, 4.0f, "%.3f");

  ImGui::Spacing();
  ImGui::SeparatorText("Emissive");

  auto emissive = sbx::math::color{material.emissive_factor.x(), material.emissive_factor.y(), material.emissive_factor.z(), 1.0f};

  if (controls::color3("Emissive Color", emissive)) {
    material.emissive_factor.x() = emissive.r();
    material.emissive_factor.y() = emissive.g();
    material.emissive_factor.z() = emissive.b();
  }

  ImGui::DragFloat("Emissive Strength", &material.emissive_strength, 0.05f, 0.0f, 100.0f, "%.3f");

  ImGui::Spacing();
  ImGui::SeparatorText("Alpha");

  controls::enum_combo("Alpha Mode", material.alpha);

  if (material.alpha == sbx::models::alpha_mode::mask) {
    ImGui::DragFloat("Alpha Cutoff", &material.alpha_cutoff, 0.005f, 0.0f, 1.0f, "%.3f");
  }

  ImGui::Checkbox("Double Sided", &material.is_double_sided);

  ImGui::Spacing();
  ImGui::SeparatorText("UV Transform");

  auto uv_scale = std::array<std::float_t, 2>{material.uv_scale.x(), material.uv_scale.y()};

  if (ImGui::DragFloat2("UV Scale", uv_scale.data(), 0.05f, 0.001f, 1000.0f, "%.3f")) {
    material.uv_scale.x() = uv_scale[0];
    material.uv_scale.y() = uv_scale[1];
  }

  auto uv_offset = std::array<std::float_t, 2>{material.uv_offset.x(), material.uv_offset.y()};

  if (ImGui::DragFloat2("UV Offset", uv_offset.data(), 0.005f, -10.0f, 10.0f, "%.3f")) {
    material.uv_offset.x() = uv_offset[0];
    material.uv_offset.y() = uv_offset[1];
  }

  ImGui::Spacing();
  ImGui::SeparatorText("Parallax");

  ImGui::DragFloat("Height Scale", &material.height_scale, 0.001f, 0.0f, 1.0f, "%.4f");
  ImGui::DragFloat("Height Offset", &material.height_offset, 0.001f, -1.0f, 1.0f, "%.4f");
  ImGui::DragFloat("Min Layers", &material.parallax_min_layers, 1.0f, 1.0f, 256.0f, "%.0f");
  ImGui::DragFloat("Max Layers", &material.parallax_max_layers, 1.0f, 1.0f, 256.0f, "%.0f");

  ImGui::Spacing();
  ImGui::SeparatorText("Sway");

  ImGui::DragFloat("Sway Speed", &material.sway_speed, 0.01f, 0.0f, 10.0f, "%.3f");
  ImGui::DragFloat("Sway Strength", &material.sway_strength, 0.005f, 0.0f, 1.0f, "%.3f");
  ImGui::DragFloat("Sway Falloff", &material.sway_falloff_exponent, 0.05f, 0.0f, 16.0f, "%.3f");

  ImGui::DragFloat("Scrumble Speed", &material.scrumble_speed, 0.01f, 0.0f, 10.0f, "%.3f");
  ImGui::DragFloat("Scrumble Strength", &material.scrumble_strength, 0.005f, 0.0f, 1.0f, "%.3f");
  ImGui::DragFloat("Scrumble Falloff", &material.scrumble_falloff_exponent, 0.05f, 0.0f, 16.0f, "%.3f");

  ImGui::Spacing();
  ImGui::SeparatorText("Features");

  auto cast_shadow = material.features.has(sbx::models::material_feature::cast_shadow);

  if (ImGui::Checkbox("Cast Shadow", &cast_shadow)) {
    if (cast_shadow) {
      material.features.set(sbx::models::material_feature::cast_shadow);
    } else {
      material.features.clear(sbx::models::material_feature::cast_shadow);
    }
  }

  auto receive_shadow = material.features.has(sbx::models::material_feature::receive_shadow);

  if (ImGui::Checkbox("Receive Shadow", &receive_shadow)) {
    if (receive_shadow) {
      material.features.set(sbx::models::material_feature::receive_shadow);
    } else {
      material.features.clear(sbx::models::material_feature::receive_shadow);
    }
  }

  auto invert_backface_normals = material.features.has(sbx::models::material_feature::invert_backface_normals);

  if (ImGui::Checkbox("Invert Backface Normals", &invert_backface_normals)) {
    if (invert_backface_normals) {
      material.features.set(sbx::models::material_feature::invert_backface_normals);
    } else {
      material.features.clear(sbx::models::material_feature::invert_backface_normals);
    }
  }

  ImGui::Spacing();
  ImGui::SeparatorText("Textures");

  const auto image_payload = asset_browser_panel::image_handle_payload();

  controls::texture_slot("Albedo", material.albedo, _texture_cache, image_payload);
  controls::texture_slot("Normal", material.normal, _texture_cache, image_payload);
  controls::texture_slot("Metallic/Roughness", material.metallic_roughness, _texture_cache, image_payload);
  controls::texture_slot("Occlusion", material.occlusion, _texture_cache, image_payload);
  controls::texture_slot("Emissive", material.emissive, _texture_cache, image_payload);
  controls::texture_slot("Height", material.height, _texture_cache, image_payload);

  ImGui::PopID();
}

} // namespace editor
