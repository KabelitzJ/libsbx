// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_NAV_CROWD_HPP_
#define LIBSBX_PHYSICS_NAV_CROWD_HPP_

#include <cstddef>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/vector3.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/physics/nav/nav_agent.hpp>
#include <libsbx/physics/nav/navmesh.hpp>
#include <libsbx/physics/nav/navmesh_query.hpp>
#include <libsbx/physics/nav/obstacle_avoidance.hpp>
#include <libsbx/physics/nav/proximity_grid.hpp>

namespace sbx::physics {

class crowd final : public utility::noncopyable {

public:

  crowd() = default;

  auto request_move_target(nav_agent& agent, const navmesh& mesh, const math::vector3& target) -> bool;

  auto update(scenes::scene& scene, const navmesh& mesh, std::float_t dt) -> void;

private:

  struct agent_frame_state {
    scenes::node node{};
    memory::observer_ptr<nav_agent> agent{};
    memory::observer_ptr<scenes::local_transform> transform{};

    bool active{false};
    math::vector3 steering_velocity{};
    math::vector3 target_velocity{};
    std::vector<scenes::node> neighbor_nodes{};
    std::vector<avoidance_circle_obstacle> neighbor_obstacles{};
  }; // struct agent_frame_state

  proximity_grid _grid{};
  std::vector<agent_frame_state> _agents{};
  std::vector<straight_path_point> _corners{};
  std::vector<math::vector3> _displacement{};

}; // class crowd

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NAV_CROWD_HPP_
