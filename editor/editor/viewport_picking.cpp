// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/viewport_picking.hpp>

#include <limits>

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

#include <editor/viewport_camera.hpp>

namespace editor {

namespace {

// Unprojects a viewport-relative pixel position into a world-space ray through camera_node.
// perspective() bakes in Vulkan's y-flip (see its implementation), so Vulkan's own NDC-to-pixel
// mapping applies unchanged here: pixel (0,0) at the viewport's top-left maps to NDC (-1,-1), no
// extra flip needed.
auto ray_from_viewport_position(sbx::scenes::node& camera_node, const sbx::scenes::camera& camera, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> sbx::math::ray {
  const auto aspect = viewport_size.y() > 0u ? static_cast<std::float_t>(viewport_size.x()) / static_cast<std::float_t>(viewport_size.y()) : 1.0f;

  const auto [view, projection] = compute_viewport_camera_matrices(camera_node, camera, aspect);
  const auto inverse_view_projection = sbx::math::matrix4x4::inverted(projection * view);

  const auto ndc_x = viewport_size.x() > 0u ? (position.x() / static_cast<std::float_t>(viewport_size.x())) * 2.0f - 1.0f : 0.0f;
  const auto ndc_y = viewport_size.y() > 0u ? (position.y() / static_cast<std::float_t>(viewport_size.y())) * 2.0f - 1.0f : 0.0f;

  const auto unproject = [&inverse_view_projection](const sbx::math::vector3f& ndc) -> sbx::math::vector3f {
    const auto point = inverse_view_projection * sbx::math::vector4{ndc, 1.0f};
    return sbx::math::vector3f{point.x(), point.y(), point.z()} / point.w();
  };

  const auto near_point = unproject(sbx::math::vector3f{ndc_x, ndc_y, 0.0f});
  const auto far_point = unproject(sbx::math::vector3f{ndc_x, ndc_y, 1.0f});

  return sbx::math::ray{near_point, far_point - near_point};
}

} // namespace

auto pick_node_at_viewport_position(editor_state& state, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  if (!scene.has_active_camera()) {
    state.clear_selection();
    return;
  }

  auto camera_node = scene.active_camera();

  if (!camera_node.is_valid() || !camera_node.has_component<sbx::scenes::camera>()) {
    state.clear_selection();
    return;
  }

  const auto& camera = camera_node.get_component<sbx::scenes::camera>();
  const auto ray = ray_from_viewport_position(camera_node, camera, position, viewport_size);

  auto closest_entity = sbx::ecs::entity{sbx::ecs::null_entity};
  auto closest_t = std::numeric_limits<std::float_t>::max();
  auto found_hit = false;

  for (const auto entity : scene.query<sbx::scenes::mesh_renderer, sbx::scenes::world_transform>()) {
    auto node = scene.node_of(entity);

    const auto& renderer = node.get_component<sbx::scenes::mesh_renderer>();

    if (!renderer.mesh.is_valid()) {
      continue;
    }

    const auto world_bounds = sbx::math::volume::transformed(renderer.mesh->bounds(), node.world_matrix());

    if (const auto hit = world_bounds.intersects(ray); hit.has_value() && *hit < closest_t) {
      closest_t = *hit;
      closest_entity = entity;
      found_hit = true;
    }
  }

  if (found_hit) {
    state.select_node(closest_entity);
  } else {
    state.clear_selection();
  }
}

} // namespace editor
