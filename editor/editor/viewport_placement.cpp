// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_placement.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <editor/editor_module.hpp>
#include <editor/node_actions.hpp>
#include <editor/surface_raycast.hpp>
#include <editor/viewport_camera.hpp>
#include <editor/viewport_picking.hpp>

#include <editor/commands/component_commands.hpp>
#include <editor/commands/composite_command.hpp>

namespace editor {

// The shortest rotation taking +Y onto @p normal.
auto rotation_onto_normal(const sbx::math::vector3& normal) -> sbx::math::quaternion {
  const auto up = sbx::math::vector3{0.0f, 1.0f, 0.0f};
  const auto direction = sbx::math::vector3::normalized(normal);
  const auto cosine = std::clamp(sbx::math::vector3::dot(up, direction), -1.0f, 1.0f);

  if (cosine > 0.9999f) {
    return sbx::math::quaternion::identity;
  }

  if (cosine < -0.9999f) {
    return sbx::math::quaternion{sbx::math::vector3{1.0f, 0.0f, 0.0f}, sbx::math::angle{sbx::math::radian{sbx::math::pif}}};
  }

  return sbx::math::quaternion{sbx::math::vector3::normalized(sbx::math::vector3::cross(up, direction)), sbx::math::angle{sbx::math::radian{std::acos(cosine)}}};
}

// The node's world position written back as a local position under its parent.
auto local_position_for(sbx::scenes::scene& scene, sbx::scenes::node& node, const sbx::math::vector3& world_position) -> sbx::math::vector3 {
  const auto parent = scene.node_of(node.get_component<sbx::scenes::relationship>().parent);
  const auto& parent_inverse = parent.has_component<sbx::scenes::world_transform>() ? parent.get_component<sbx::scenes::world_transform>().inverse : sbx::math::matrix4x4::identity;

  return sbx::math::vector3{parent_inverse * sbx::math::vector4{world_position, 1.0f}};
}

auto viewport_drop_placement(editor_state& state, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> drop_placement {
  auto& scene = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();

  if (const auto ray = viewport_ray(position, viewport_size)) {
    if (const auto hit = raycast_surface(scene, *ray)) {
      return drop_placement{hit->point, state.align_drops_to_surface ? std::optional{rotation_onto_normal(hit->normal)} : std::nullopt};
    }
  }

  return drop_placement{viewport_drop_position(position, viewport_size), std::nullopt};
}

auto snap_selection_to_floor(editor_state& state, sbx::scenes::scene& scene) -> void {
  const auto roots = selection_roots(scene, state.selected_node_ids());

  auto commands = std::vector<std::unique_ptr<command>>{};

  for (const auto id : roots) {
    auto node = scene.find(id);

    if (!node.is_valid()) {
      continue;
    }

    // Each root on its own; the whole selection is ignored below so nodes don't land on each other.
    const auto bounds = subtree_bounds(scene, std::span<const sbx::math::uuid>{&id, 1u});

    if (bounds.is_empty()) {
      continue;
    }

    const auto bottom = sbx::math::vector3{bounds.center().x(), bounds.min().y(), bounds.center().z()};
    const auto ray = sbx::math::ray{bottom + sbx::math::vector3{0.0f, 0.01f, 0.0f}, sbx::math::vector3{0.0f, -1.0f, 0.0f}};

    const auto hit = raycast_surface(scene, ray, roots);

    if (!hit) {
      continue;
    }

    const auto drop = hit->point.y() - bounds.min().y();
    const auto& world = node.get_component<sbx::scenes::world_transform>();
    const auto world_position = sbx::math::vector3{world.matrix[3]} + sbx::math::vector3{0.0f, drop, 0.0f};

    const auto before = node.get_component<sbx::scenes::local_transform>();
    auto after = before;
    after.position = local_position_for(scene, node, world_position);

    commands.push_back(std::make_unique<modify_component_command<sbx::scenes::local_transform>>(node.id(), before, after, "Snap to Floor"));
  }

  if (!commands.empty()) {
    state.push_command(scene, std::make_unique<composite_command>(std::move(commands), "Snap to Floor"));
  }
}

// The world-space vertex of a node's mesh nearest a reference, by distance to a point or (for the anchor) to the cursor ray.
template<typename Distance>
auto nearest_mesh_vertex(sbx::scenes::node& node, Distance&& distance) -> std::optional<sbx::math::vector3> {
  const auto renderer = node.try_get_component<sbx::scenes::mesh_renderer>();

  if (!renderer || !renderer->mesh.is_valid()) {
    return std::nullopt;
  }

  const auto& data = surface_mesh_data(renderer->mesh);
  const auto& world = node.get_component<sbx::scenes::world_transform>().matrix;

  auto best = std::optional<sbx::math::vector3>{};
  auto best_distance = std::numeric_limits<std::float_t>::max();

  for (const auto& vertex : data.vertices) {
    const auto world_vertex = sbx::math::vector3{world * sbx::math::vector4{vertex, 1.0f}};

    if (const auto candidate = distance(world_vertex); candidate < best_distance) {
      best_distance = candidate;
      best = world_vertex;
    }
  }

  return best;
}

auto update_vertex_snap(editor_state& state, const ImVec2& image_origin, const sbx::math::vector2u& viewport_size, bool image_clicked) -> bool {
  auto& drag = state.vertex_snap;
  auto& scene = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();

  const auto mouse = ImGui::GetMousePos();
  const auto ray = viewport_ray(sbx::math::vector2{mouse.x - image_origin.x, mouse.y - image_origin.y}, viewport_size);

  // Start: V held and a click on the image with something selected.
  if (!drag.is_active) {
    if (!image_clicked || !ImGui::IsKeyDown(ImGuiKey_V) || ImGui::GetIO().WantTextInput || state.selected_node_count() == 0u || !ray) {
      return false;
    }

    auto primary = state.selected_node(scene);

    if (!primary.is_valid()) {
      return false;
    }

    // The anchor is the selection's vertex nearest the cursor ray, or the node's origin without a mesh.
    const auto distance_to_ray = [&ray](const sbx::math::vector3& point) {
      const auto direction = sbx::math::vector3::normalized(ray->direction());
      const auto offset = point - ray->origin();

      return (offset - direction * sbx::math::vector3::dot(offset, direction)).length();
    };

    const auto& primary_world = primary.get_component<sbx::scenes::world_transform>().matrix;

    drag.anchor = nearest_mesh_vertex(primary, distance_to_ray).value_or(sbx::math::vector3{primary_world[3]});
    drag.start_positions.clear();
    drag.before.clear();

    for (const auto id : selection_roots(scene, state.selected_node_ids())) {
      if (auto node = scene.find(id); node.is_valid()) {
        drag.start_positions.emplace(id, sbx::math::vector3{node.get_component<sbx::scenes::world_transform>().matrix[3]});
        drag.before.emplace(id, node.get_component<sbx::scenes::local_transform>());
      }
    }

    drag.is_active = !drag.start_positions.empty();

    return drag.is_active;
  }

  // Release: one undo step from the start to where the drag left the nodes.
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    auto commands = std::vector<std::unique_ptr<command>>{};

    for (const auto& [id, before] : drag.before) {
      if (auto node = scene.find(id); node.is_valid()) {
        commands.push_back(std::make_unique<modify_component_command<sbx::scenes::local_transform>>(id, before, node.get_component<sbx::scenes::local_transform>(), "Vertex Snap"));
      }
    }

    if (!commands.empty()) {
      state.push_command(scene, std::make_unique<composite_command>(std::move(commands), "Vertex Snap"));
    }

    drag = editor_state::vertex_snap_drag{};

    return true;
  }

  if (!ray) {
    return true;
  }

  // The target: the vertex nearest the cursor's surface hit on another node, or the hit itself on a heightfield.
  auto ignored = std::vector<sbx::math::uuid>{};

  for (const auto& [id, position] : drag.start_positions) {
    ignored.push_back(id);
  }

  const auto hit = raycast_surface(scene, *ray, ignored);

  if (!hit) {
    return true;
  }

  auto target_node = hit->node;
  const auto target = nearest_mesh_vertex(target_node, [&hit](const sbx::math::vector3& point) { return (point - hit->point).length(); }).value_or(hit->point);
  const auto offset = target - drag.anchor;

  for (const auto& [id, start] : drag.start_positions) {
    if (auto node = scene.find(id); node.is_valid()) {
      node.get_component<sbx::scenes::local_transform>().position = local_position_for(scene, node, start + offset);
    }
  }

  return true;
}

auto update_ruler(editor_state& state, const ImVec2& image_origin, const sbx::math::vector2u& viewport_size, bool image_clicked) -> bool {
  if (!state.is_ruler_active) {
    state.ruler_points.clear();
    return false;
  }

  auto& scene = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();

  if (image_clicked) {
    const auto mouse = ImGui::GetMousePos();

    if (const auto ray = viewport_ray(sbx::math::vector2{mouse.x - image_origin.x, mouse.y - image_origin.y}, viewport_size)) {
      if (const auto hit = raycast_surface(scene, *ray)) {
        if (state.ruler_points.size() >= 2u) {
          state.ruler_points.clear();
        }

        state.ruler_points.push_back(hit->point);
      }
    }
  }

  const auto pose = sbx::core::engine::get_module<editor_module>().viewport_camera(scene);

  if (!pose || viewport_size.x() == 0u || viewport_size.y() == 0u) {
    return true;
  }

  const auto matrices = compute_viewport_camera_matrices(pose->world_matrix, pose->params, static_cast<std::float_t>(viewport_size.x()) / static_cast<std::float_t>(viewport_size.y()));
  const auto view_projection = matrices.projection * matrices.view;

  const auto to_screen = [&](const sbx::math::vector3& point) -> std::optional<ImVec2> {
    const auto projected = project_to_viewport_position(view_projection, point, viewport_size);

    return projected.visible ? std::optional{ImVec2{image_origin.x + projected.position.x(), image_origin.y + projected.position.y()}} : std::nullopt;
  };

  auto* draw_list = ImGui::GetWindowDrawList();
  constexpr auto color = IM_COL32(255, 210, 60, 255);

  for (const auto& point : state.ruler_points) {
    if (const auto screen = to_screen(point)) {
      draw_list->AddCircleFilled(*screen, 4.0f, color);
    }
  }

  if (state.ruler_points.size() == 2u) {
    const auto a = to_screen(state.ruler_points[0]);
    const auto b = to_screen(state.ruler_points[1]);

    if (a && b) {
      draw_list->AddLine(*a, *b, color, 2.0f);

      const auto label = fmt::format("{:.3f} m", (state.ruler_points[1] - state.ruler_points[0]).length());
      const auto middle = ImVec2{(a->x + b->x) * 0.5f, (a->y + b->y) * 0.5f};
      const auto size = ImGui::CalcTextSize(label.c_str());

      draw_list->AddRectFilled(ImVec2{middle.x - 4.0f, middle.y - size.y - 6.0f}, ImVec2{middle.x + size.x + 4.0f, middle.y - 2.0f}, IM_COL32(20, 20, 20, 200), 3.0f);
      draw_list->AddText(ImVec2{middle.x, middle.y - size.y - 4.0f}, color, label.c_str());
    }
  }

  return true;
}

} // namespace editor
