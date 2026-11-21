// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/physics/character_controller.hpp>
#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/physics_module.hpp>

namespace scenes = sbx::scenes;
namespace physics = sbx::physics;
namespace math = sbx::math;

auto make_wall(scenes::scene& scene) -> scenes::node {
  auto wall = scene.create_node("Wall", scenes::local_transform{math::vector3{2.0f, 0.0f, 0.0f}});
  wall.add_component<physics::shape_collider>(physics::shape_collider{.shape = physics::box{math::vector3{1.0f, 2.0f, 0.5f}}});

  scene.update();

  return wall;
}

TEST(broadphase_contributor, untouched_collider_is_unchanged) {
  auto scene = scenes::scene{"Test"};
  auto wall = make_wall(scene);

  const auto record = physics::snapshot_broadphase_contributor(wall);

  scene.update();

  EXPECT_TRUE(physics::is_unchanged_broadphase_contributor(record));
}

TEST(broadphase_contributor, moving_the_node_is_a_change_before_and_after_the_transform_update) {
  auto scene = scenes::scene{"Test"};
  auto wall = make_wall(scene);

  const auto record = physics::snapshot_broadphase_contributor(wall);

  wall.transform().position = math::vector3{3.0f, 0.0f, 0.0f};
  EXPECT_FALSE(physics::is_unchanged_broadphase_contributor(record));

  scene.update();
  EXPECT_FALSE(physics::is_unchanged_broadphase_contributor(record));

  EXPECT_TRUE(physics::is_unchanged_broadphase_contributor(physics::snapshot_broadphase_contributor(wall)));
}

TEST(broadphase_contributor, moving_an_ancestor_is_a_change_after_the_transform_update) {
  auto scene = scenes::scene{"Test"};
  auto parent = scene.create_node("Building");
  auto wall = make_wall(scene);
  wall.set_parent(parent);
  scene.update();

  const auto record = physics::snapshot_broadphase_contributor(wall);

  parent.transform().position = math::vector3{0.0f, 0.0f, 10.0f};
  scene.update();

  EXPECT_FALSE(physics::is_unchanged_broadphase_contributor(record));
}

TEST(broadphase_contributor, editing_or_removing_the_collider_is_a_change) {
  auto scene = scenes::scene{"Test"};
  auto wall = make_wall(scene);

  const auto record = physics::snapshot_broadphase_contributor(wall);

  // Friction doesn't move the bounds.
  wall.get_component<physics::shape_collider>().friction = 0.9f;
  EXPECT_TRUE(physics::is_unchanged_broadphase_contributor(record));

  wall.get_component<physics::shape_collider>().shape = physics::box{math::vector3{1.0f, 4.0f, 0.5f}};
  EXPECT_FALSE(physics::is_unchanged_broadphase_contributor(record));

  const auto resized = physics::snapshot_broadphase_contributor(wall);
  static_cast<void>(wall.remove_component<physics::shape_collider>());
  EXPECT_FALSE(physics::is_unchanged_broadphase_contributor(resized));
}

TEST(broadphase_contributor, controller_is_never_unchanged) {
  auto scene = scenes::scene{"Test"};
  auto unit = scene.create_node("Unit");
  unit.add_component<physics::character_controller>();
  scene.update();

  EXPECT_FALSE(physics::is_unchanged_broadphase_contributor(physics::snapshot_broadphase_contributor(unit)));
}

TEST(broadphase_contributor, destroyed_node_is_a_change) {
  auto scene = scenes::scene{"Test"};
  auto wall = make_wall(scene);

  const auto record = physics::snapshot_broadphase_contributor(wall);

  scene.destroy_node(wall);

  EXPECT_FALSE(physics::is_unchanged_broadphase_contributor(record));
}
