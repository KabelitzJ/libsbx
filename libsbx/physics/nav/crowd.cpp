// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/nav/crowd.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <libsbx/math/quaternion.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>

#include <libsbx/physics/nav/local_boundary.hpp>
#include <libsbx/physics/nav/navmesh_query.hpp>
#include <libsbx/physics/nav/obstacle_avoidance.hpp>
#include <libsbx/physics/nav/path_corridor.hpp>

namespace sbx::physics {

inline constexpr auto max_crowd_neighbors = std::size_t{6};

auto crowd::request_move_target(nav_agent& agent, const navmesh& mesh, const math::vector3& target) -> bool {
  const auto start_ref = find_nearest_poly(mesh, agent.corridor.position);
  const auto end_ref = find_nearest_poly(mesh, target);

  if (start_ref == null_poly_reference || end_ref == null_poly_reference) {
    agent.state = nav_agent_state::target_unreachable;

    return false;
  }

  const auto result = find_path(mesh, start_ref, end_ref, agent.corridor.position, target);

  if (!result.success) {
    agent.state = nav_agent_state::target_unreachable;

    return false;
  }

  corridor_set_corridor(agent.corridor, target, result.polys);

  agent.target = target;
  agent.state = nav_agent_state::moving;
  agent.arrival_settle_timer = 0.0f;

  return true;
}

struct agent_frame_state {
  scenes::node node{};
  nav_agent* agent{nullptr};
  scenes::local_transform* transform{nullptr};

  bool active{false};
  math::vector3 steering_velocity{};
  math::vector3 target_velocity{};
  std::vector<scenes::node> neighbor_nodes{};
  std::vector<avoidance_circle_obstacle> neighbor_obstacles{};
}; // struct agent_frame_state

auto crowd::update(scenes::scene& scene, const navmesh& mesh, std::float_t dt) -> void {
  _grid.clear();

  auto agents = std::vector<agent_frame_state>{};

  for (auto&& [entity, agent, transform] : scene.query<nav_agent, scenes::local_transform>(ecs::exclude<scenes::inactive>).each()) {
    auto node = scene.node_of(entity);

    _grid.insert(node, transform.position);

    agents.push_back(agent_frame_state{node, &agent, &transform});
  }

  for (auto& state : agents) {
    auto& agent = *state.agent;
    auto& transform = *state.transform;

    if (agent.corridor.path.empty()) {
      corridor_reset(agent.corridor, find_nearest_poly(mesh, transform.position), transform.position);
    }

    if (agent.state == nav_agent_state::target_unreachable) {
      agent.target_replan_timer += dt;

      if (agent.target_replan_timer >= 1.0f) {
        agent.target_replan_timer = 0.0f;
        request_move_target(agent, mesh, agent.corridor.target);
      }
    }

    if (agent.state != nav_agent_state::moving) {
      continue;
    }

    const auto corners = corridor_find_corners(agent.corridor, mesh, 4);

    if (corners.empty()) {
      agent.state = nav_agent_state::idle;
      agent.velocity = math::vector3::zero;

      continue;
    }

    const auto optimize_target_index = std::min(std::size_t{1}, corners.size() - 1);
    optimize_path_visibility(agent.corridor, mesh, corners[optimize_target_index].position, agent.path_optimization_range);

    const auto& next_corner = corners.front();
    const auto to_corner = math::vector3{next_corner.position.x() - transform.position.x(), 0.0f, next_corner.position.z() - transform.position.z()};
    const auto distance_to_corner = to_corner.length();

    const auto& final_corner = corners.back();
    const auto at_end_of_path = final_corner.reference == null_poly_reference;
    const auto slow_down_radius = agent.radius * 2.0f;

    auto distance_to_goal = slow_down_radius;
    auto raw_distance_to_final = slow_down_radius;

    if (at_end_of_path) {
      const auto to_final = math::vector3{final_corner.position.x() - transform.position.x(), 0.0f, final_corner.position.z() - transform.position.z()};
      raw_distance_to_final = to_final.length();
      distance_to_goal = std::min(raw_distance_to_final, slow_down_radius);
    }

    const auto has_arrived = at_end_of_path && raw_distance_to_final <= agent.radius;
    const auto settle_speed = agent.max_speed * 0.1f;
    const auto is_settling = at_end_of_path && raw_distance_to_final <= slow_down_radius && agent.velocity.length_squared() <= settle_speed * settle_speed;

    agent.arrival_settle_timer = is_settling ? (agent.arrival_settle_timer + dt) : 0.0f;

    if (has_arrived || agent.arrival_settle_timer >= 0.5f) {
      agent.state = nav_agent_state::idle;
      agent.velocity = math::vector3::zero;
      agent.arrival_settle_timer = 0.0f;

      continue;
    }

    const auto desired_direction = (distance_to_corner > 0.0001f) ? (to_corner / distance_to_corner) : math::vector3::zero;
    const auto speed_scale = distance_to_goal / slow_down_radius;

    state.steering_velocity = desired_direction * (agent.max_speed * speed_scale);

    if (agent.obstacle_avoidance_enabled || agent.separation_enabled) {
      const auto boundary_update_threshold = agent.collision_query_range * 0.25f;
      const auto boundary_dx = transform.position.x() - agent.boundary.center.x();
      const auto boundary_dz = transform.position.z() - agent.boundary.center.z();

      if ((boundary_dx * boundary_dx + boundary_dz * boundary_dz) > boundary_update_threshold * boundary_update_threshold) {
        local_boundary_update(agent.boundary, mesh, agent.corridor.path.front(), transform.position, agent.collision_query_range);
      }

      auto neighbor_candidates = std::vector<std::pair<std::float_t, scenes::node>>{};

      state.neighbor_nodes.clear();
      _grid.query(transform.position, agent.collision_query_range, state.neighbor_nodes);

      const auto range_sqr = agent.collision_query_range * agent.collision_query_range;

      for (const auto& neighbor_node : state.neighbor_nodes) {
        if (neighbor_node == state.node) {
          continue;
        }

        const auto neighbor_agent = neighbor_node.try_get_component<nav_agent>();
        const auto neighbor_transform = neighbor_node.try_get_component<scenes::local_transform>();

        if (!neighbor_agent || !neighbor_transform) {
          continue;
        }

        const auto dx = transform.position.x() - neighbor_transform->position.x();
        const auto dz = transform.position.z() - neighbor_transform->position.z();
        const auto vertical_gap = std::abs(transform.position.y() - neighbor_transform->position.y());

        if (vertical_gap >= (agent.height + neighbor_agent->height) * 0.5f) {
          continue;
        }

        const auto distance_sqr = dx * dx + dz * dz;

        if (distance_sqr > range_sqr) {
          continue;
        }

        neighbor_candidates.push_back({distance_sqr, neighbor_node});
      }

      std::sort(neighbor_candidates.begin(), neighbor_candidates.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

      if (neighbor_candidates.size() > max_crowd_neighbors) {
        neighbor_candidates.resize(max_crowd_neighbors);
      }

      state.neighbor_nodes.clear();
      state.neighbor_obstacles.clear();

      for (const auto& [distance_sqr, neighbor_node] : neighbor_candidates) {
        const auto neighbor_agent = neighbor_node.try_get_component<nav_agent>();
        const auto neighbor_transform = neighbor_node.try_get_component<scenes::local_transform>();

        state.neighbor_nodes.push_back(neighbor_node);
        state.neighbor_obstacles.push_back(avoidance_circle_obstacle{neighbor_transform->position, neighbor_agent->velocity, neighbor_agent->radius});
      }

      if (agent.separation_enabled) {
        state.steering_velocity = state.steering_velocity + compute_separation_velocity(transform.position, state.neighbor_obstacles, agent.collision_query_range, agent.separation_weight);

        const auto steering_speed_sqr = state.steering_velocity.length_squared();
        const auto max_speed_sqr = agent.max_speed * agent.max_speed;

        if (steering_speed_sqr > max_speed_sqr) {
          state.steering_velocity = state.steering_velocity * (max_speed_sqr / steering_speed_sqr);
        }
      }
    }

    state.active = true;
  }

  for (auto& state : agents) {
    if (!state.active) {
      continue;
    }

    auto& agent = *state.agent;

    state.target_velocity = agent.obstacle_avoidance_enabled
      ? sample_avoidance_velocity(state.transform->position, agent.velocity, state.steering_velocity, agent.radius, agent.max_speed, state.neighbor_obstacles, agent.boundary.segments)
      : state.steering_velocity;
  }

  for (auto& state : agents) {
    if (!state.active) {
      continue;
    }

    auto& agent = *state.agent;
    auto& transform = *state.transform;

    const auto velocity_delta = state.target_velocity - agent.velocity;
    const auto max_delta = agent.max_acceleration * dt;
    const auto delta_length = velocity_delta.length();

    agent.velocity = (delta_length > max_delta && delta_length > 0.0001f)
      ? agent.velocity + velocity_delta * (max_delta / delta_length)
      : state.target_velocity;

    transform.position = transform.position + agent.velocity * dt;
  }

  auto displacement = std::vector<math::vector3>(agents.size());

  for (auto iteration = 0; iteration < 4; ++iteration) {
    std::fill(displacement.begin(), displacement.end(), math::vector3::zero);

    for (auto i = std::size_t{0}; i < agents.size(); ++i) {
      auto& state = agents[i];

      if (!state.active) {
        continue;
      }

      const auto& agent = *state.agent;
      const auto& transform = *state.transform;

      auto total = math::vector3::zero;
      auto count = 0.0f;

      for (const auto& neighbor_node : state.neighbor_nodes) {
        const auto neighbor_agent = neighbor_node.try_get_component<nav_agent>();
        const auto neighbor_transform = neighbor_node.try_get_component<scenes::local_transform>();

        if (!neighbor_agent || !neighbor_transform) {
          continue;
        }

        auto diff = math::vector3{transform.position.x() - neighbor_transform->position.x(), 0.0f, transform.position.z() - neighbor_transform->position.z()};
        const auto combined_radius = agent.radius + neighbor_agent->radius;
        const auto distance_squared = diff.length_squared();

        if (distance_squared > combined_radius * combined_radius) {
          continue;
        }

        auto penetration = 0.0f;

        if (distance_squared < 0.00000001f) {
          diff = (state.node.id().value() > neighbor_node.id().value())
            ? math::vector3{-state.steering_velocity.z(), 0.0f, state.steering_velocity.x()}
            : math::vector3{state.steering_velocity.z(), 0.0f, -state.steering_velocity.x()};
          penetration = 0.01f;
        } else {
          const auto distance = std::sqrt(distance_squared);
          penetration = (1.0f / distance) * ((combined_radius - distance) * 0.5f) * 0.7f;
        }

        total = total + diff * penetration;
        count += 1.0f;
      }

      if (count > 0.0001f) {
        displacement[i] = total * (1.0f / count);
      }
    }

    for (auto i = std::size_t{0}; i < agents.size(); ++i) {
      if (agents[i].active) {
        agents[i].transform->position = agents[i].transform->position + displacement[i];
      }
    }
  }

  for (auto& state : agents) {
    if (!state.active) {
      continue;
    }

    auto& agent = *state.agent;
    auto& transform = *state.transform;

    const auto new_position = corridor_move_position(agent.corridor, mesh, transform.position);

    transform.position = new_position + math::vector3{0.0f, agent.base_offset, 0.0f};

    if (agent.velocity.length_squared() > 0.0001f) {
      transform.rotation = math::quaternion::look_at(math::vector3::normalized(agent.velocity));
    }
  }
}

} // namespace sbx::physics
