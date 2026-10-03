// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/noise.hpp>

#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/physics_module.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>
#include <libsbx/physics/nav/nav_settings.hpp>
#include <libsbx/physics/nav/navmesh.hpp>

#include <libsbx/terrain/terrain_module.hpp>

#include <libsbx/scenes/components.hpp>

namespace sbx::scripting {

auto interop::physics_raycast(math::ray* ray, std::float_t max_distance, std::uint32_t layer_mask, std::uint64_t* out_node_uuid, math::vector3* out_point, math::vector3* out_normal, std::float_t* out_distance) -> managed::bool32 {
  if (!ray) {
    utility::logger<"scripting">::error("Attempting to call physics_raycast with a null ray");

    return false;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& physics_module = core::engine::get_module<physics::physics_module>();

  const auto hit = physics_module.raycast(scene, *ray, max_distance, scenes::layer_mask{layer_mask});

  if (!hit) {
    return false;
  }

  if (out_node_uuid) { 
    *out_node_uuid = hit->node.id().value(); 
  }

  if (out_point) { 
    *out_point = hit->point; 
  }

  if (out_normal) { 
    *out_normal = hit->normal; 
  }

  if (out_distance) {
    *out_distance = hit->distance;
  }

  return true;
}

auto interop::nav_bake(std::float_t agent_radius, std::float_t agent_height, std::float_t agent_max_slope, std::float_t agent_max_climb, std::float_t cell_size, std::float_t cell_height, std::float_t region_min_size, std::float_t region_merge_size, std::float_t edge_max_length, std::float_t edge_max_error, std::int32_t verts_per_poly) -> managed::bool32 {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto& physics_module = core::engine::get_module<physics::physics_module>();

  auto settings = physics::nav_settings{};
  settings.agent_radius = agent_radius;
  settings.agent_height = agent_height;
  settings.agent_max_slope = agent_max_slope;
  settings.agent_max_climb = agent_max_climb;
  settings.cell_size = cell_size;
  settings.cell_height = cell_height;
  settings.region_min_size = region_min_size;
  settings.region_merge_size = region_merge_size;
  settings.edge_max_length = edge_max_length;
  settings.edge_max_error = edge_max_error;
  settings.verts_per_poly = verts_per_poly;

  return physics_module.bake_navmesh(scene, settings);
}

auto interop::nav_has_navmesh() -> managed::bool32 {
  auto& physics_module = core::engine::get_module<physics::physics_module>();

  return physics_module.has_navmesh();
}

auto interop::nav_sample_position(math::vector3* point, math::vector3* out_result) -> managed::bool32 {
  if (!point) {
    utility::logger<"scripting">::error("Attempting to call nav_sample_position with a null point");

    return false;
  }

  auto& physics_module = core::engine::get_module<physics::physics_module>();

  if (!physics_module.has_navmesh()) {
    return false;
  }

  const auto& mesh = physics_module.navmesh();
  const auto ref = physics::find_nearest_poly(mesh, *point);

  if (ref == physics::null_poly_reference) {
    return false;
  }

  if (out_result) {
    *out_result = physics::closest_point_on_poly(mesh, ref, *point);
  }

  return true;
}

auto interop::nav_agent_set_destination(std::uint64_t uuid, math::vector3* target) -> managed::bool32 {
  if (!target) {
    utility::logger<"scripting">::error("Attempting to call nav_agent_set_destination with a null target");

    return false;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent destination of invalid node");

    return false;
  }

  auto& physics_module = core::engine::get_module<physics::physics_module>();

  return physics_module.request_agent_move(node, *target);
}

auto interop::nav_agent_get_state(std::uint64_t uuid) -> std::uint8_t {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to get nav_agent state of invalid node");

    return 0u;
  }

  return static_cast<std::uint8_t>(node.get_component<physics::nav_agent>().state);
}

auto interop::nav_agent_get_velocity(std::uint64_t uuid, math::vector3* out_velocity) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_velocity) {
    utility::logger<"scripting">::error("Attempting to get nav_agent velocity of invalid node");

    return;
  }

  *out_velocity = node.get_component<physics::nav_agent>().velocity;
}

auto interop::nav_agent_get_remaining_distance(std::uint64_t uuid, std::float_t* out_distance) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_distance) {
    utility::logger<"scripting">::error("Attempting to get nav_agent remaining distance of invalid node");

    return;
  }

  const auto& agent = node.get_component<physics::nav_agent>();

  *out_distance = math::vector3::distance(agent.corridor.position, agent.corridor.target);
}

auto interop::nav_agent_get_radius(std::uint64_t uuid, std::float_t* out_radius) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_radius) {
    utility::logger<"scripting">::error("Attempting to get nav_agent radius of invalid node");

    return;
  }

  *out_radius = node.get_component<physics::nav_agent>().radius;
}

auto interop::nav_agent_set_radius(std::uint64_t uuid, std::float_t radius) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent radius of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().radius = radius;
}

auto interop::nav_agent_get_height(std::uint64_t uuid, std::float_t* out_height) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_height) {
    utility::logger<"scripting">::error("Attempting to get nav_agent height of invalid node");

    return;
  }

  *out_height = node.get_component<physics::nav_agent>().height;
}

auto interop::nav_agent_set_height(std::uint64_t uuid, std::float_t height) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent height of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().height = height;
}

auto interop::nav_agent_get_base_offset(std::uint64_t uuid, std::float_t* out_base_offset) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_base_offset) {
    utility::logger<"scripting">::error("Attempting to get nav_agent base offset of invalid node");

    return;
  }

  *out_base_offset = node.get_component<physics::nav_agent>().base_offset;
}

auto interop::nav_agent_set_base_offset(std::uint64_t uuid, std::float_t base_offset) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent base offset of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().base_offset = base_offset;
}

auto interop::nav_agent_get_speed(std::uint64_t uuid, std::float_t* out_speed) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_speed) {
    utility::logger<"scripting">::error("Attempting to get nav_agent speed of invalid node");

    return;
  }

  *out_speed = node.get_component<physics::nav_agent>().max_speed;
}

auto interop::nav_agent_set_speed(std::uint64_t uuid, std::float_t speed) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent speed of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().max_speed = speed;
}

auto interop::nav_agent_get_acceleration(std::uint64_t uuid, std::float_t* out_acceleration) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid() || !out_acceleration) {
    utility::logger<"scripting">::error("Attempting to get nav_agent acceleration of invalid node");

    return;
  }

  *out_acceleration = node.get_component<physics::nav_agent>().max_acceleration;
}

auto interop::nav_agent_set_acceleration(std::uint64_t uuid, std::float_t acceleration) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to set nav_agent acceleration of invalid node");

    return;
  }

  node.get_component<physics::nav_agent>().max_acceleration = acceleration;
}

auto interop::terrain_generate(std::uint32_t width, std::uint32_t depth, std::float_t cell_size, std::float_t frequency, std::float_t amplitude, std::uint32_t octaves) -> void {
  auto& terrain_module = core::engine::get_module<terrain::terrain_module>();

  terrain_module.generate(terrain::heightmap_generator_settings{width, depth, cell_size, frequency, amplitude, octaves});
}

auto interop::terrain_sample_height(math::vector2* world_xz, std::float_t* out_height) -> void {
  if (!world_xz || !out_height) {
    utility::logger<"scripting">::error("Attempting to call terrain_sample_height with a null pointer");

    return;
  }

  auto& terrain_module = core::engine::get_module<terrain::terrain_module>();

  *out_height = terrain_module.sample_height(*world_xz);
}

auto interop::terrain_sample_normal(math::vector2* world_xz, math::vector3* out_normal) -> void {
  if (!world_xz || !out_normal) {
    utility::logger<"scripting">::error("Attempting to call terrain_sample_normal with a null pointer");

    return;
  }

  auto& terrain_module = core::engine::get_module<terrain::terrain_module>();

  *out_normal = terrain_module.sample_normal(*world_xz);
}

auto interop::math_noise_simplex(std::float_t x, std::float_t y, std::float_t z) -> std::float_t {
  return math::noise::simplex(x, y, z);
}

auto interop::math_noise_fractal(std::float_t x, std::float_t y, std::float_t z, std::uint32_t octaves, std::float_t lacunarity, std::float_t gain) -> std::float_t {
  return math::noise::fractal(x, y, z, octaves, lacunarity, gain);
}

} // namespace sbx::scripting
