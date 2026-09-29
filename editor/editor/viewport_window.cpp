// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_window.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/ui/ui_module.hpp>
#include <libsbx/render/ui/widgets/asset_tile.hpp>

#include <editor/editor_ui_layer.hpp>
#include <editor/editor_module.hpp>

#include <editor/viewport_transform_gizmo.hpp>
#include <editor/viewport_view_gizmo.hpp>
#include <editor/viewport_overlays.hpp>
#include <editor/viewport_picking.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/scene_commands.hpp>

#include <editor/panels/hierarchy_prefab_menu.hpp>

namespace editor {

// Mesh -> new node at the drop point, prefab -> instance at the drop point, material -> assigned to every submesh slot of the mesh under the cursor.
auto accept_viewport_asset_drop(editor_state& state, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> void {
  auto& scene = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto drag_of = [](const ImGuiPayload* payload) -> const sbx::render::asset_drag_payload& {
    return *static_cast<const sbx::render::asset_drag_payload*>(payload->Data);
  };

  if (const auto* payload = ImGui::AcceptDragDropPayload(sbx::render::drag_drop_payload_mesh)) {
    const auto& drag = drag_of(payload);
    auto command = std::make_unique<create_mesh_node_command>(drag.id, std::filesystem::path{drag.path}.stem().string(), std::nullopt, viewport_drop_position(position, viewport_size));
    auto* created = command.get();

    state.push_command(scene, std::move(command));
    state.select_node(scene.find(created->id()));
  }

  if (ImGui::GetDragDropPayload() != nullptr && ImGui::GetDragDropPayload()->IsDataType(sbx::render::drag_drop_payload_prefab)) {
    try_instantiate_prefab_drop(state, scene, std::nullopt, viewport_drop_position(position, viewport_size));
  }

  if (const auto* payload = ImGui::AcceptDragDropPayload(sbx::render::drag_drop_payload_material)) {
    const auto ray = viewport_ray(position, viewport_size);
    const auto hit = ray ? raycast_nodes(*ray) : std::nullopt;

    if (!hit) {
      return;
    }

    auto node = hit->node;
    auto& renderer = node.get_component<sbx::scenes::mesh_renderer>();
    const auto before = renderer;

    const auto material = assets_module.load_material(drag_of(payload).id);
    std::ranges::fill(renderer.materials, material);

    state.push_command(scene, std::make_unique<modify_component_command<sbx::scenes::mesh_renderer>>(node.id(), before, renderer, "Assign Material"));
  }
}

auto draw_viewport_window(editor_state& state, const sbx::graphics::sampler& sampler) -> bool {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::Begin(editor_ui_layer::viewport_window_name, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // ImGui::PopStyleVar();

  const auto is_hovered = ImGui::IsWindowHovered();

  auto available = ImGui::GetContentRegionAvail();

  auto width = static_cast<std::uint32_t>(available.x > 0.0f ? available.x : 1.0f);
  auto height = static_cast<std::uint32_t>(available.y > 0.0f ? available.y : 1.0f);

  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  const auto final_image = scene_renderer_module.final_image();

  if (final_image.is_valid() && available.x > 0.0f && available.y > 0.0f) {
    scene_renderer_module.set_viewport_extent(sbx::math::vector2u{width, height});

    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& registry = graphics_module.resource_registry();

    const auto texture_id = ui_module.texture_id(registry.get<sbx::graphics::image>(final_image).view(), sampler);

    ImGui::Image(texture_id, available);

    const auto image_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const auto image_origin = ImGui::GetItemRectMin();

    scene_renderer_module.set_viewport_offset(sbx::math::vector2{image_origin.x, image_origin.y});

    auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();
    const auto is_editing = editor_module.play_state() == editor::play_state::edit;

    if (is_editing && ImGui::BeginDragDropTarget()) {
      const auto mouse_position = ImGui::GetMousePos();

      accept_viewport_asset_drop(state, sbx::math::vector2{mouse_position.x - image_origin.x, mouse_position.y - image_origin.y}, sbx::math::vector2u{width, height});

      ImGui::EndDragDropTarget();
    }

    auto gizmo_active = false;
    auto toolbar_active = false;
    auto view_gizmo_active = false;
    auto icons_active = false;

    if (is_editing) {
      gizmo_active = draw_viewport_gizmo(state, image_origin, available);
      toolbar_active = draw_gizmo_toolbar(state, image_origin);
      view_gizmo_active = draw_view_gizmo(image_origin, available);
      icons_active = draw_node_icons(state, image_origin, available, gizmo_active);
      draw_camera_frustum_gizmo(state, available);
    }

    if (is_editing && image_clicked && !gizmo_active && !toolbar_active && !view_gizmo_active && !icons_active) {
      const auto mouse_position = ImGui::GetMousePos();

      pick_node_at_viewport_position(state, sbx::math::vector2{mouse_position.x - image_origin.x, mouse_position.y - image_origin.y}, sbx::math::vector2u{width, height});
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();

  return is_hovered;
}

} // namespace editor
