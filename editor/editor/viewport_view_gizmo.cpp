// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_view_gizmo.hpp>

#include <array>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <optional>
#include <utility>

#include <ImGuizmo.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>

#include <editor/editor_module.hpp>
#include <editor/viewport_camera.hpp>

namespace editor {

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

} // namespace editor
