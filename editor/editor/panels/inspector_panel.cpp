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
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <editor/bindings/imgui.hpp>

#include <editor/panels/asset_browser_panel.hpp>

#include <editor/widgets/controls.hpp>

namespace editor {

inspector_panel::inspector_panel(texture_cache& cache)
: _texture_cache{cache} {
  _register_default_components();
}

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

  _draw_tag(selected_node);
  _draw_transform(selected_node);
  _draw_components(selected_node);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  _draw_add_component_button(selected_node);

  ImGui::End();
}

auto inspector_panel::_register_default_components() -> void {
  _components.register_component<sbx::scenes::directional_light>(
    ICON_MDI_LIGHTBULB " Directional Light",
    true,
    [this](sbx::scenes::node node, sbx::scenes::directional_light& light) -> void {
      _draw_directional_light(node, light);
    },
    []() -> sbx::scenes::directional_light {
      return sbx::scenes::directional_light{sbx::math::color::white(), sbx::math::vector3::down};
    }
  );

  _components.register_component<sbx::scenes::point_light>(
    ICON_MDI_LIGHTBULB " Point Light",
    true,
    [this](sbx::scenes::node node, sbx::scenes::point_light& light) -> void {
      _draw_point_light(node, light);
    },
    []() -> sbx::scenes::point_light {
      return sbx::scenes::point_light{sbx::math::color::white(), 5.0f};
    }
  );

  _components.register_component<sbx::scenes::camera>(
    ICON_MDI_CAMERA " Camera",
    true,
    [this](sbx::scenes::node node, sbx::scenes::camera& cam) -> void {
      _draw_camera(node, cam);
    },
    []() -> sbx::scenes::camera {
      return sbx::scenes::camera{sbx::math::angle{sbx::math::degree{60.0f}}, 0.1f, 1000.0f};
    }
  );

  // No factory: static_mesh requires a mesh and material asset and must be added through code or serialization.
  _components.register_component<sbx::scenes::static_mesh>(
    ICON_MDI_VECTOR_POLYGON " Static Mesh",
    true,
    [this](sbx::scenes::node node, sbx::scenes::static_mesh& mesh) -> void {
      _draw_static_mesh(node, mesh);
    },
    []() -> sbx::scenes::static_mesh {
      return sbx::scenes::static_mesh{sbx::math::uuid::nil(), sbx::math::uuid::nil()};
    }
  );
}

auto inspector_panel::_draw_components(sbx::scenes::node node) -> void {
  for (const auto& entry : _components.entries()) {
    if (!entry.has(node)) {
      continue;
    }

    ImGui::PushID(static_cast<std::int32_t>(entry.id));

    auto is_visible = true;
    auto* visible_ptr = entry.is_removable ? &is_visible : nullptr;

    const auto is_expanded = ImGui::CollapsingHeader(entry.label.c_str(), visible_ptr, ImGuiTreeNodeFlags_DefaultOpen);

    if (is_expanded && is_visible) {
      entry.draw(node);
    }

    if (entry.is_removable && !is_visible) {
      entry.remove(node);
    }

    ImGui::PopID();
  }
}

auto inspector_panel::_draw_add_component_button(sbx::scenes::node node) -> void {
  static constexpr auto popup_id = "##add_component_popup";

  const auto avail_width = ImGui::GetContentRegionAvail().x;
  const auto button_width = std::min(avail_width, 220.0f);
  const auto offset = (avail_width - button_width) * 0.5f;

  if (offset > 0.0f) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
  }

  if (ImGui::Button(ICON_MDI_PLUS " Add Component", ImVec2{button_width, 0.0f})) {
    ImGui::OpenPopup(popup_id);
  }

  if (ImGui::BeginPopup(popup_id)) {
    auto any_available = false;

    for (const auto& entry : _components.entries()) {
      if (!entry.factory) {
        continue;
      }

      if (entry.has(node)) {
        continue;
      }

      any_available = true;

      if (ImGui::MenuItem(entry.label.c_str())) {
        entry.factory(node);
      }
    }

    if (!any_available) {
      ImGui::TextDisabled("No components to add");
    }

    ImGui::EndPopup();
  }
}

auto inspector_panel::_draw_tag(sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_TAG " Tag###tag", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& graph = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene().graph();

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

auto inspector_panel::_draw_transform(sbx::scenes::node node) -> void {
  if (!ImGui::CollapsingHeader(ICON_MDI_AXIS_ARROW " Transform###transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  auto& graph = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene().graph();

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

auto inspector_panel::_draw_directional_light(sbx::scenes::node node, sbx::scenes::directional_light& light) -> void {
  static_cast<void>(node);

  auto direction = light.direction();

  if (controls::vector3("Direction", direction)) {
    light.set_direction(direction);
  }

  auto& color = light.color();

  auto rgb = sbx::math::color{color.r(), color.g(), color.b(), 1.0f};

  if (controls::color3("Color", rgb)) {
    color.r() = rgb.r();
    color.g() = rgb.g();
    color.b() = rgb.b();
  }

  ImGui::DragFloat("Intensity", &color.a(), 0.1f, 0.0f, 10000.0f, "%.2f");
}

auto inspector_panel::_draw_point_light(sbx::scenes::node node, sbx::scenes::point_light& light) -> void {
  static_cast<void>(node);

  auto& color = light.color();

  auto rgb = sbx::math::color{color.r(), color.g(), color.b(), 1.0f};

  if (controls::color3("Color##point_light", rgb)) {
    color.r() = rgb.r();
    color.g() = rgb.g();
    color.b() = rgb.b();
  }

  ImGui::DragFloat("Intensity##point_light", &color.a(), 0.1f, 0.0f, 10000.0f, "%.2f");

  ImGui::DragFloat("Radius##point_light", &light.radius(), 0.1f, 0.0f, 10000.0f, "%.2f");
}

auto inspector_panel::_draw_camera(sbx::scenes::node node, sbx::scenes::camera& cam) -> void {
  static_cast<void>(node);

  auto fov = cam.field_of_view().to_degrees().value();

  if (ImGui::DragFloat("Field of View", &fov, 0.5f, 1.0f, 179.0f, "%.1f deg")) {
    cam.set_field_of_view(sbx::math::angle{sbx::math::degree{fov}});
  }

  controls::labeled_text("Near", "{:.3f}", cam.near_plane());
  controls::labeled_text("Far", "{:.1f}", cam.far_plane());
}

auto inspector_panel::_draw_static_mesh(sbx::scenes::node node, sbx::scenes::static_mesh& mesh) -> void {
  static_cast<void>(node);

  controls::labeled_text("Mesh", "{}", mesh.mesh_id());
  controls::labeled_text("Submeshes", "{}", mesh.submeshes().size());

  ImGui::Separator();

  const auto& submeshes = mesh.submeshes();

  for (auto i = 0u; i < submeshes.size(); ++i) {
    const auto& submesh = submeshes[i];

    if (submesh.material == sbx::math::uuid::nil()) {
      continue;
    }

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

  controls::labeled_text("Name", "{:s}", metadata.name.c_str());
  controls::labeled_text("UUID", "{}", material_id);

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
