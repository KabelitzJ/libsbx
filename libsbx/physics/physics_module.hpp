// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
#define LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_

#include <cmath>
#include <optional>
#include <vector>
#include <algorithm>
#include <ranges>
#include <unordered_map>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/module.hpp>

#include <libsbx/containers/octree.hpp>

#include <libsbx/math/constants.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/global_transform.hpp>
#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>

#include <libsbx/physics/shape_collider.hpp>
#include <libsbx/physics/collision_detection.hpp>
#include <libsbx/physics/rigidbody.hpp>

namespace sbx::physics {

class physics_module : public core::module<physics_module> {

  inline static const auto is_registered = register_module(stage::fixed, dependencies<scenes::scenes_module>{});

public:

  physics_module() = default;

  ~physics_module() override = default;

  auto update() -> void override;

private:

  using octree_type = containers::octree<scenes::node, 16u, 8u>;
  using collision_pair_type = typename octree_type::intersection;

  struct collision {
    scenes::node node_a;
    scenes::node node_b;
    collision_manifold manifold;
  }; // struct collision

  struct contact_constraint {
    math::vector3 normal;
    math::vector3 tangent1;
    math::vector3 tangent2;
    math::vector3 r_a;
    math::vector3 r_b;
    std::float_t normal_mass{0.0f};
    std::float_t tangent1_mass{0.0f};
    std::float_t tangent2_mass{0.0f};
    std::float_t bias{0.0f};
    std::float_t normal_impulse{0.0f};
    std::float_t tangent1_impulse{0.0f};
    std::float_t tangent2_impulse{0.0f};
    std::uint64_t last_seen{0};
  }; // struct contact_constraint

  struct solver_contact {
    scenes::node node_a{};
    scenes::node node_b{};
    std::size_t key{};
    contact_constraint constraint{};
  }; // struct solver_contact

  auto _integrate_velocities(std::float_t dt) -> void;

  auto _integrate_positions(std::float_t dt) -> void;

  auto _collision_broad_phase() -> std::vector<collision_pair_type>;

  auto _collision_narrow_phase(const std::vector<collision_pair_type>& pairs) -> std::vector<collision>;

  auto _resolve_collisions(const std::vector<collision>& collisions, std::float_t dt) -> void;

  auto _update_character_controllers() -> void;

  auto _update_cache(const std::vector<solver_contact>& solver_contacts) -> void;

  auto _positional_correction(const std::vector<collision>& collisions) -> void;

  std::unordered_map<std::size_t, contact_constraint> _contact_cache{};
  std::uint64_t _solver_tick{0};

}; // class physics_module

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
