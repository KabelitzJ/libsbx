// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_picking.hpp>

#include <limits>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/ray.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <editor/editor_module.hpp>
#include <editor/viewport_camera.hpp>

namespace editor {

// Unprojects a viewport pixel into a world-space ray; perspective() already includes Vulkan's y-flip.
auto ray_from_viewport_position(const sbx::math::matrix4x4& camera_world_matrix, const sbx::scenes::camera& camera, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> sbx::math::ray {
  const auto aspect = viewport_size.y() > 0u ? static_cast<std::float_t>(viewport_size.x()) / static_cast<std::float_t>(viewport_size.y()) : 1.0f;

  const auto [view, projection] = compute_viewport_camera_matrices(camera_world_matrix, camera, aspect);
  const auto inverse_view_projection = sbx::math::matrix4x4::inverted(projection * view);

  const auto ndc_x = viewport_size.x() > 0u ? (position.x() / static_cast<std::float_t>(viewport_size.x())) * 2.0f - 1.0f : 0.0f;
  const auto ndc_y = viewport_size.y() > 0u ? (position.y() / static_cast<std::float_t>(viewport_size.y())) * 2.0f - 1.0f : 0.0f;

  const auto unproject = [&inverse_view_projection](const sbx::math::vector3& ndc) -> sbx::math::vector3 {
    const auto point = inverse_view_projection * sbx::math::vector4{ndc, 1.0f};
    return sbx::math::vector3{point.x(), point.y(), point.z()} / point.w();
  };

  const auto near_point = unproject(sbx::math::vector3{ndc_x, ndc_y, 0.0f});
  const auto far_point = unproject(sbx::math::vector3{ndc_x, ndc_y, 1.0f});

  return sbx::math::ray{near_point, far_point - near_point};
}


auto project_to_viewport_position(const sbx::math::matrix4x4& view_projection, const sbx::math::vector3& world_position, const sbx::math::vector2u& viewport_size) -> viewport_projection {
  const auto clip = view_projection * sbx::math::vector4{world_position, 1.0f};

  if (clip.w() <= 0.0f) {
    return viewport_projection{};
  }

  const auto ndc_x = clip.x() / clip.w();
  const auto ndc_y = clip.y() / clip.w();

  if (ndc_x < -1.0f || ndc_x > 1.0f || ndc_y < -1.0f || ndc_y > 1.0f) {
    return viewport_projection{};
  }

  return viewport_projection{
    true,
    sbx::math::vector2{
      (ndc_x + 1.0f) * 0.5f * static_cast<std::float_t>(viewport_size.x()),
      (ndc_y + 1.0f) * 0.5f * static_cast<std::float_t>(viewport_size.y())
    }
  };
}

auto viewport_ray(const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> std::optional<sbx::math::ray> {
  auto& scene = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();
  const auto pose = sbx::core::engine::get_module<editor::editor_module>().viewport_camera(scene);

  if (!pose) {
    return std::nullopt;
  }

  return ray_from_viewport_position(pose->world_matrix, pose->params, position, viewport_size);
}

auto raycast_nodes(const sbx::math::ray& ray) -> std::optional<viewport_hit> {
  auto& scene = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();

  auto closest_node = sbx::scenes::node{};
  auto closest_t = std::numeric_limits<std::float_t>::max();

  for (const auto entity : scene.query<sbx::scenes::mesh_renderer, sbx::scenes::world_transform>(sbx::ecs::exclude<sbx::scenes::inactive>)) {
    auto node = scene.node_of(entity);

    const auto& renderer = node.get_component<sbx::scenes::mesh_renderer>();

    if (!renderer.mesh.is_valid()) {
      continue;
    }

    const auto world_bounds = sbx::math::volume::transformed(renderer.mesh->bounds(), node.world_matrix());

    if (const auto hit = world_bounds.intersects(ray); hit.has_value() && *hit < closest_t) {
      closest_t = *hit;
      closest_node = node;
    }
  }

  if (!closest_node.is_valid()) {
    return std::nullopt;
  }

  return viewport_hit{closest_node, ray.point_at(closest_t)};
}

auto viewport_drop_position(const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> sbx::math::vector3 {
  const auto ray = viewport_ray(position, viewport_size);

  if (!ray) {
    return sbx::math::vector3{0.0f, 0.0f, 0.0f};
  }

  if (const auto hit = raycast_nodes(*ray)) {
    return hit->position;
  }

  // The y = 0 plane when the ray points down, otherwise a fixed distance ahead.
  const auto direction_y = ray->direction().y();

  if (direction_y < -1e-4f) {
    return ray->point_at(-ray->origin().y() / direction_y);
  }

  return ray->origin() + sbx::math::vector3::normalized(ray->direction()) * 10.0f;
}

auto pick_node_at_viewport_position(editor_state& state, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> void {
  const auto ray = viewport_ray(position, viewport_size);
  const auto hit = ray ? raycast_nodes(*ray) : std::nullopt;

  if (hit) {
    const auto& closest_node = hit->node;

    if (ImGui::GetIO().KeyCtrl) {
      state.toggle_node_selection(closest_node);
    } else if (ImGui::GetIO().KeyShift) {
      state.add_node_to_selection(closest_node);
    } else {
      state.select_node(closest_node);
    }
  } else if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
    state.clear_selection();
  }
}

} // namespace editor
