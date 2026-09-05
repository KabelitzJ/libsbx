// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_gizmo.hpp>

#include <array>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include <ImGuizmo.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/ecs/view.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

#include <editor/editor_module.hpp>
#include <editor/viewport_camera.hpp>

#include <editor/commands/component_commands.hpp>

namespace editor {

auto to_imguizmo_operation(gizmo_operation operation) -> ImGuizmo::OPERATION {
  switch (operation) {
    case gizmo_operation::translate: return ImGuizmo::TRANSLATE;
    case gizmo_operation::rotate: return ImGuizmo::ROTATE;
    case gizmo_operation::scale: return ImGuizmo::SCALE;
  }

  return ImGuizmo::TRANSLATE;
}

auto to_imguizmo_mode(gizmo_operation operation, gizmo_mode mode) -> ImGuizmo::MODE {
  if (operation == gizmo_operation::scale) {
    return ImGuizmo::LOCAL;
  }

  return mode == gizmo_mode::local ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
}

auto handle_operation_shortcuts(editor_state& state) -> void {
  if (!ImGui::IsWindowHovered()) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
    state.current_gizmo_operation = gizmo_operation::translate;
  } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
    state.current_gizmo_operation = gizmo_operation::rotate;
  } else if (ImGui::IsKeyPressed(ImGuiKey_3, false)) {
    state.current_gizmo_operation = gizmo_operation::scale;
  }
}

auto draw_viewport_gizmo(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool {
  handle_operation_shortcuts(state);

  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return false;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = state.selected_node(scene);

  if (!node.is_valid()) {
    return false;
  }

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  const auto pose = editor_module.viewport_camera(scene);

  if (!pose) {
    return false;
  }

  ImGuizmo::SetDrawlist();
  ImGuizmo::SetRect(viewport_origin.x, viewport_origin.y, viewport_size.x, viewport_size.y);

  const auto aspect = viewport_size.x / viewport_size.y;
  const auto matrices = compute_viewport_camera_matrices(pose->world_matrix, pose->params, aspect);

  auto gizmo_projection = matrices.projection;
  gizmo_projection[1][1] *= -1.0f;

  const auto operation = to_imguizmo_operation(state.current_gizmo_operation);
  const auto mode = to_imguizmo_mode(state.current_gizmo_operation, state.current_gizmo_mode);

  auto world_matrix = node.world_matrix();

  // Hold Ctrl to snap (Blender/Unity convention) instead of moving freely. ImGuizmo reads snap
  // as 3 per-axis values for translate/scale, or just snap[0] (degrees) for rotate.
  static constexpr auto translate_snap = std::array<std::float_t, 3u>{1.0f, 1.0f, 1.0f};
  static constexpr auto rotate_snap = std::array<std::float_t, 3u>{15.0f, 15.0f, 15.0f};
  static constexpr auto scale_snap = std::array<std::float_t, 3u>{0.1f, 0.1f, 0.1f};

  const std::float_t* snap = nullptr;

  if (ImGui::GetIO().KeyCtrl) {
    switch (state.current_gizmo_operation) {
      case gizmo_operation::translate: snap = translate_snap.data(); break;
      case gizmo_operation::rotate: snap = rotate_snap.data(); break;
      case gizmo_operation::scale: snap = scale_snap.data(); break;
    }
  }

  // Captured before Manipulate() runs this frame, so it's the true pre-drag value even on the
  // exact frame IsUsing() first flips true (see the false->true check below).
  const auto pre_manipulate = node.transform();

  const auto changed = ImGuizmo::Manipulate(matrices.view.data(), gizmo_projection.data(), operation, mode, world_matrix.data(), nullptr, snap);

  // Cross-frame gizmo-drag state — one shared instance is enough since only one node can have an
  // active gizmo drag at a time. Brackets the per-frame write below into one undo entry per drag.
  static auto drag_active = false;
  static auto drag_node_id = sbx::math::uuid::nil();
  static auto drag_before = sbx::scenes::local_transform{};

  if (ImGuizmo::IsUsing() && !drag_active) {
    drag_active = true;
    drag_node_id = node.id();
    drag_before = pre_manipulate;
  }

  if (changed) {
    const auto& relationship = node.get_component<sbx::scenes::relationship>();

    auto local_matrix = world_matrix;

    if (relationship.parent != sbx::ecs::null_entity) {
      if (auto parent_node = scene.node_of(relationship.parent); parent_node.is_valid() && parent_node.has_component<sbx::scenes::world_transform>()) {
        local_matrix = sbx::math::matrix4x4::inverted(parent_node.world_matrix()) * world_matrix;
      }
    }

    auto translation = std::array<std::float_t, 3u>{};
    auto rotation = std::array<std::float_t, 3u>{}; // degrees
    auto scale = std::array<std::float_t, 3u>{};

    ImGuizmo::DecomposeMatrixToComponents(local_matrix.data(), translation.data(), rotation.data(), scale.data());

    auto& transform = node.transform();
    transform.position = sbx::math::vector3{translation[0], translation[1], translation[2]};
    transform.rotation = sbx::math::quaternion{sbx::math::vector3{rotation[0], rotation[1], rotation[2]}};
    transform.scale = sbx::math::vector3{scale[0], scale[1], scale[2]};
  }

  if (!ImGuizmo::IsUsing() && drag_active) {
    drag_active = false;

    if (auto dragged_node = scene.find(drag_node_id); dragged_node.is_valid()) {
      state.push_command(std::make_unique<modify_component_command<sbx::scenes::local_transform>>(drag_node_id, drag_before, dragged_node.transform(), "Edit Transform"));
    }
  }

  return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

auto draw_gizmo_toolbar(editor_state& state, const ImVec2& viewport_origin) -> bool {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  if (!state.selected_node(scene).is_valid()) {
    return false;
  }

  constexpr auto padding = 8.0f;
  const auto button_size = ImVec2{28.0f, 28.0f};

  ImGui::SetCursorScreenPos(ImVec2{viewport_origin.x + padding, viewport_origin.y + padding});

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{4.0f, 4.0f});
  ImGui::BeginGroup();

  const auto tool_button = [&](const char* icon, gizmo_operation operation, const char* tooltip) {
    const auto is_active = state.current_gizmo_operation == operation;

    if (is_active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    }

    if (ImGui::Button(icon, button_size)) {
      state.current_gizmo_operation = operation;
    }

    if (is_active) {
      ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltip);
    }
  };

  tool_button(ICON_MDI_ARROW_ALL, gizmo_operation::translate, "Translate (1)");
  tool_button(ICON_MDI_ROTATE_3D_VARIANT, gizmo_operation::rotate, "Rotate (2)");
  tool_button(ICON_MDI_ARROW_EXPAND_ALL, gizmo_operation::scale, "Scale (3)");

  ImGui::EndGroup();
  ImGui::PopStyleVar();

  return ImGui::IsItemHovered();
}

struct axis_gizmo_handle {
  ImVec2 position;
  std::float_t depth;
  ImU32 color;
  const char* label; // nullptr for the negative end (drawn as a hollow ring, no letter)
  sbx::math::vector3 look_from_direction;
}; // struct axis_gizmo_handle

// In-flight camera-snap transition. File-local rather than in editor_state — purely an
// implementation detail of draw_view_gizmo's corner widget, not a cross-panel concern.
struct camera_snap_animation {
  bool active{false};
  sbx::math::vector3 start_position{};
  sbx::math::quaternion start_rotation{sbx::math::quaternion::identity};
  sbx::math::vector3 target_position{};
  sbx::math::quaternion target_rotation{sbx::math::quaternion::identity};
  std::float_t elapsed{0.0f};
}; // struct camera_snap_animation

constexpr auto camera_snap_duration = 0.25f; // seconds

// Computes the world position/rotation for the editor camera looking at the world origin from
// `direction * length` — the interpolation target for camera_snap_animation. Plain world space,
// no parent-relative conversion needed: the editor camera isn't a scene node, so it never has one.
auto compute_camera_snap_target(const sbx::math::vector3& direction, std::float_t length) -> std::pair<sbx::math::vector3, sbx::math::quaternion> {
  const auto up = std::fabs(direction.y()) > 0.99f ? sbx::math::vector3{0.0f, 0.0f, 1.0f} : sbx::math::vector3{0.0f, 1.0f, 0.0f};

  const auto eye = direction * length;
  const auto new_view = sbx::math::matrix4x4::look_at(eye, sbx::math::vector3{0.0f, 0.0f, 0.0f}, up);

  const auto camera_world_matrix = sbx::math::matrix4x4::inverted(new_view);

  auto translation = std::array<std::float_t, 3u>{};
  auto rotation = std::array<std::float_t, 3u>{}; // degrees
  auto scale = std::array<std::float_t, 3u>{};

  ImGuizmo::DecomposeMatrixToComponents(camera_world_matrix.data(), translation.data(), rotation.data(), scale.data());

  return {
    sbx::math::vector3{translation[0], translation[1], translation[2]},
    sbx::math::quaternion{sbx::math::vector3{rotation[0], rotation[1], rotation[2]}}
  };
}

auto draw_view_gizmo(const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool {
  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return false;
  }

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  // Edit-time navigation aid only — during Play this would be fighting whatever's actually
  // driving the scene's own play camera (scripts, physics, ...).
  if (editor_module.play_state() != editor::play_state::edit) {
    return false;
  }

  auto& camera = editor_module.editor_camera();

  const auto aspect = viewport_size.x / viewport_size.y;
  const auto matrices = compute_viewport_camera_matrices(camera.world_matrix(), camera.params(), aspect);

  // No orbit-pivot concept on this fly camera, so distance-from-origin is the closest stand-in
  // for how far out to place the camera when snapping to a clicked axis.
  const auto camera_position = camera.transform().position;
  const auto length = std::max(camera_position.length(), 1.0f);

  constexpr auto padding = 8.0f;
  constexpr auto widget_extent = 90.0f;
  const auto center = ImVec2{
    viewport_origin.x + viewport_size.x - widget_extent * 0.5f - padding,
    viewport_origin.y + widget_extent * 0.5f + padding
  };

  constexpr auto radius = 32.0f;
  constexpr auto handle_radius = 8.0f;
  constexpr auto handle_radius_negative = 5.0f;
  constexpr auto label_size = 13.0f;

  struct axis_definition {
    sbx::math::vector3 direction;
    ImU32 color;
    const char* label;
  }; // struct axis_definition

  static constexpr auto axes = std::array<axis_definition, 3u>{
    axis_definition{sbx::math::vector3{1.0f, 0.0f, 0.0f}, IM_COL32(219, 61, 61, 255), "X"},
    axis_definition{sbx::math::vector3{0.0f, 1.0f, 0.0f}, IM_COL32(90, 191, 90, 255), "Y"},
    axis_definition{sbx::math::vector3{0.0f, 0.0f, 1.0f}, IM_COL32(64, 120, 219, 255), "Z"}
  };

  auto handles = std::array<axis_gizmo_handle, 6u>{};

  for (auto i = std::size_t{0u}; i < axes.size(); ++i) {
    const auto& axis = axes[i];
    const auto view_direction = matrices.view * sbx::math::vector4{axis.direction, 0.0f};
    const auto screen_offset = ImVec2{view_direction.x() * radius, -view_direction.y() * radius};

    handles[i * 2u + 0u] = axis_gizmo_handle{
      ImVec2{center.x + screen_offset.x, center.y + screen_offset.y},
      view_direction.z(),
      axis.color,
      axis.label,
      axis.direction
    };

    handles[i * 2u + 1u] = axis_gizmo_handle{
      ImVec2{center.x - screen_offset.x, center.y - screen_offset.y},
      -view_direction.z(),
      axis.color,
      nullptr,
      sbx::math::vector3{-axis.direction.x(), -axis.direction.y(), -axis.direction.z()}
    };
  }

  // Back-to-front so nearer handles draw, and hit-test, on top of farther ones.
  auto order = std::array<std::size_t, 6u>{0u, 1u, 2u, 3u, 4u, 5u};
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return handles[a].depth < handles[b].depth; });

  auto* draw_list = ImGui::GetWindowDrawList();
  auto* font = ImGui::GetFont();

  draw_list->AddCircleFilled(center, radius + handle_radius, IM_COL32(26, 26, 26, 60));

  auto snapped_direction = std::optional<sbx::math::vector3>{};
  auto active = false;

  for (const auto index : order) {
    const auto& handle = handles[index];
    const auto is_positive = handle.label != nullptr;
    const auto handle_size = is_positive ? handle_radius : handle_radius_negative;

    if (is_positive) {
      draw_list->AddLine(center, handle.position, IM_COL32(255, 255, 255, 60), 1.5f);
    }

    ImGui::SetCursorScreenPos(ImVec2{handle.position.x - handle_size, handle.position.y - handle_size});
    ImGui::PushID(static_cast<std::int32_t>(index));
    ImGui::InvisibleButton("##axis_handle", ImVec2{handle_size * 2.0f, handle_size * 2.0f});

    const auto hovered = ImGui::IsItemHovered();
    active |= hovered || ImGui::IsItemActive();

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      snapped_direction = handle.look_from_direction;
    }

    ImGui::PopID();

    const auto fill_color = hovered ? IM_COL32(255, 255, 255, 255) : handle.color;

    if (is_positive) {
      draw_list->AddCircleFilled(handle.position, handle_size, fill_color);

      const auto text_size = font->CalcTextSizeA(label_size, FLT_MAX, 0.0f, handle.label);
      draw_list->AddText(font, label_size, ImVec2{handle.position.x - text_size.x * 0.5f, handle.position.y - text_size.y * 0.5f}, IM_COL32(20, 20, 20, 255), handle.label);
    } else {
      draw_list->AddCircle(handle.position, handle_size, fill_color, 0, 1.5f);
    }
  }

  static auto animation = camera_snap_animation{};

  if (snapped_direction.has_value()) {
    const auto& transform = camera.transform();

    animation.start_position = transform.position;
    animation.start_rotation = transform.rotation;
    std::tie(animation.target_position, animation.target_rotation) = compute_camera_snap_target(*snapped_direction, length);
    animation.elapsed = 0.0f;
    animation.active = true;
  }

  if (animation.active) {
    animation.elapsed += sbx::core::engine::delta_time().value();

    const auto t = std::clamp(animation.elapsed / camera_snap_duration, 0.0f, 1.0f);

    auto& transform = camera.transform();
    transform.position = sbx::math::vector3::lerp(animation.start_position, animation.target_position, t);
    transform.rotation = sbx::math::quaternion::slerp(animation.start_rotation, animation.target_rotation, t);

    if (t >= 1.0f) {
      animation.active = false;
    }
  }

  return active;
}

struct icon_projection {
  bool visible{false};
  ImVec2 screen_position{};
}; // struct icon_projection

// Forward version of viewport_picking.cpp's ray_from_viewport_position: world position -> clip ->
// NDC -> pixel, rejecting anything behind the camera or outside the frustum.
auto project_to_screen(const sbx::math::matrix4x4& view_projection, const sbx::math::vector3& world_position, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> icon_projection {
  const auto clip = view_projection * sbx::math::vector4{world_position, 1.0f};

  if (clip.w() <= 0.0f) {
    return icon_projection{};
  }

  const auto ndc_x = clip.x() / clip.w();
  const auto ndc_y = clip.y() / clip.w();

  if (ndc_x < -1.0f || ndc_x > 1.0f || ndc_y < -1.0f || ndc_y > 1.0f) {
    return icon_projection{};
  }

  return icon_projection{
    true,
    ImVec2{viewport_origin.x + (ndc_x + 1.0f) * 0.5f * viewport_size.x, viewport_origin.y + (ndc_y + 1.0f) * 0.5f * viewport_size.y}
  };
}

auto draw_node_icons(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size, bool gizmo_capturing_input) -> bool {
  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return false;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  const auto pose = editor_module.viewport_camera(scene);

  if (!pose) {
    return false;
  }

  const auto aspect = viewport_size.x / viewport_size.y;
  const auto matrices = compute_viewport_camera_matrices(pose->world_matrix, pose->params, aspect);
  const auto view_projection = matrices.projection * matrices.view;

  constexpr auto icon_size = 24.0f;

  auto* font = ImGui::GetIO().FontDefault;
  auto* draw_list = ImGui::GetWindowDrawList();

  auto any_active = false;

  const auto draw_icon = [&](sbx::ecs::entity entity, const sbx::math::vector3& world_position, const char* icon) {
    const auto projected = project_to_screen(view_projection, world_position, viewport_origin, viewport_size);

    if (!projected.visible) {
      return;
    }

    const auto text_size = font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
    const auto icon_min = ImVec2{projected.screen_position.x - text_size.x * 0.5f, projected.screen_position.y - text_size.y * 0.5f};

    ImGui::SetCursorScreenPos(icon_min);
    ImGui::PushID(static_cast<std::int32_t>(entity));

    const auto node = scene.node_of(entity);
    auto hovered = false;

    // Skip hit-testing (glyph still draws) when the gizmo already has the cursor — see
    // gizmo_capturing_input's doc comment in viewport_gizmo.hpp.
    if (!gizmo_capturing_input) {
      ImGui::InvisibleButton("##node_icon", text_size);

      hovered = ImGui::IsItemHovered();

      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        state.select_node(node);
      }

      any_active |= hovered || ImGui::IsItemActive();
    }

    ImGui::PopID();

    const auto color = state.is_node_selected(node) ? IM_COL32(255, 200, 60, 255) : hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 220, 220, 200);

    draw_list->AddText(font, icon_size, icon_min, color, icon);
  };

  for (auto&& [entity, transform, light] : scene.query<sbx::scenes::world_transform, sbx::scenes::point_light>(sbx::ecs::exclude<sbx::scenes::mesh_renderer>).each()) {
    draw_icon(entity, sbx::math::vector3{transform.matrix[3]}, ICON_MDI_LIGHTBULB);
  }

  for (auto&& [entity, transform, light] : scene.query<sbx::scenes::world_transform, sbx::scenes::spot_light>(sbx::ecs::exclude<sbx::scenes::mesh_renderer>).each()) {
    draw_icon(entity, sbx::math::vector3{transform.matrix[3]}, ICON_MDI_FLASHLIGHT);
  }

  for (auto&& [entity, transform, light] : scene.query<sbx::scenes::world_transform, sbx::scenes::directional_light>(sbx::ecs::exclude<sbx::scenes::mesh_renderer>).each()) {
    draw_icon(entity, sbx::math::vector3{transform.matrix[3]}, ICON_MDI_WHITE_BALANCE_SUNNY);
  }

  // Excluded from its own icon only while actually being viewed through (Play/Paused) — there it
  // sits at the eye position, a degenerate projection (clip.w near zero) that flickered on and
  // off every other frame. In edit mode the viewport looks through the editor camera instead.
  const auto is_viewing_through_active_camera = editor_module.play_state() != editor::play_state::edit;
  const auto active_camera_id = (is_viewing_through_active_camera && scene.has_active_camera()) ? static_cast<sbx::math::uuid>(scene.active_camera().id()) : sbx::math::uuid::nil();

  for (auto&& [entity, transform, node_camera] : scene.query<sbx::scenes::world_transform, sbx::scenes::camera>(sbx::ecs::exclude<sbx::scenes::mesh_renderer>).each()) {
    if (scene.node_of(entity).id() == active_camera_id) {
      continue;
    }

    draw_icon(entity, sbx::math::vector3{transform.matrix[3]}, ICON_MDI_CAMERA);
  }

  return any_active;
}

auto draw_camera_frustum_gizmo(editor_state& state, const ImVec2& viewport_size) -> void {
  if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return;
  }

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = state.selected_node(scene);

  if (!node.is_valid() || !node.has_component<sbx::scenes::camera>()) {
    return;
  }

  const auto& camera = node.get_component<sbx::scenes::camera>();
  const auto world = node.world_matrix();
  const auto aspect = viewport_size.x / viewport_size.y;

  const auto half_fov = sbx::math::to_radians(sbx::math::degree{camera.fov_degrees}).value() * 0.5f;
  const auto half_height_near = camera.near_plane * std::tan(half_fov);
  const auto half_width_near = half_height_near * aspect;
  const auto half_height_far = camera.far_plane * std::tan(half_fov);
  const auto half_width_far = half_height_far * aspect;

  // Camera-local space, forward along -Z (matches the engine's convention elsewhere, e.g. lighting/view matrices).
  const auto near_corners = std::array<sbx::math::vector3, 4u>{
    sbx::math::vector3{-half_width_near,  half_height_near, -camera.near_plane},
    sbx::math::vector3{ half_width_near,  half_height_near, -camera.near_plane},
    sbx::math::vector3{ half_width_near, -half_height_near, -camera.near_plane},
    sbx::math::vector3{-half_width_near, -half_height_near, -camera.near_plane}
  };

  const auto far_corners = std::array<sbx::math::vector3, 4u>{
    sbx::math::vector3{-half_width_far,  half_height_far, -camera.far_plane},
    sbx::math::vector3{ half_width_far,  half_height_far, -camera.far_plane},
    sbx::math::vector3{ half_width_far, -half_height_far, -camera.far_plane},
    sbx::math::vector3{-half_width_far, -half_height_far, -camera.far_plane}
  };

  const auto to_world = [&world](const sbx::math::vector3& local) -> sbx::math::vector3 {
    const auto point = world * sbx::math::vector4{local, 1.0f};
    return sbx::math::vector3{point.x(), point.y(), point.z()};
  };

  auto& debug_draw = sbx::core::engine::get_module<sbx::render::scene_renderer_module>().debug_draw();
  const auto color = sbx::math::color::yellow();

  for (auto i = std::size_t{0u}; i < 4u; ++i) {
    const auto next = (i + 1u) % 4u;

    debug_draw.add_line(to_world(near_corners[i]), to_world(near_corners[next]), color);
    debug_draw.add_line(to_world(far_corners[i]), to_world(far_corners[next]), color);
    debug_draw.add_line(to_world(near_corners[i]), to_world(far_corners[i]), color);
  }
}

} // namespace editor
