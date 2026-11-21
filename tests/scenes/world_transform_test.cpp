// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>

namespace scenes = sbx::scenes;
namespace math = sbx::math;

auto expect_near(const math::matrix4x4& actual, const math::matrix4x4& expected) -> void {
  for (auto column = std::size_t{0u}; column < 4u; ++column) {
    for (auto row = std::size_t{0u}; row < 4u; ++row) {
      EXPECT_NEAR(actual[column][row], expected[column][row], 1e-4f) << "column " << column << ", row " << row;
    }
  }
}

TEST(world_transform, untouched_nodes_keep_their_version) {
  auto scene = scenes::scene{"Test"};
  auto node = scene.create_node("Node", scenes::local_transform{math::vector3{1.0f, 2.0f, 3.0f}});

  scene.update();
  const auto version = node.get_component<scenes::world_transform>().version;

  scene.update();
  EXPECT_EQ(node.get_component<scenes::world_transform>().version, version);
}

TEST(world_transform, moving_a_parent_recomputes_its_children) {
  auto scene = scenes::scene{"Test"};
  auto parent = scene.create_node("Parent");
  auto child = scene.create_node("Child", scenes::local_transform{math::vector3{0.0f, 1.0f, 0.0f}});
  child.set_parent(parent);

  scene.update();
  const auto version = child.get_component<scenes::world_transform>().version;

  parent.transform().position = math::vector3{5.0f, 0.0f, 0.0f};
  scene.update();

  const auto& world = child.get_component<scenes::world_transform>();
  EXPECT_NE(world.version, version);
  EXPECT_NEAR(world.matrix[3][0], 5.0f, 1e-5f);
  EXPECT_NEAR(world.matrix[3][1], 1.0f, 1e-5f);
}

TEST(world_transform, reparenting_without_moving_recomputes) {
  auto scene = scenes::scene{"Test"};
  auto first = scene.create_node("First", scenes::local_transform{math::vector3{1.0f, 0.0f, 0.0f}});
  auto second = scene.create_node("Second", scenes::local_transform{math::vector3{0.0f, 0.0f, 7.0f}});
  auto child = scene.create_node("Child");
  child.set_parent(first);

  scene.update();
  child.set_parent(second);
  scene.update();

  EXPECT_NEAR(child.get_component<scenes::world_transform>().matrix[3][2], 7.0f, 1e-5f);
}

TEST(world_transform, inverse_matches_the_matrix) {
  auto scene = scenes::scene{"Test"};

  auto parent = scene.create_node("Parent", scenes::local_transform{
    .position = math::vector3{3.0f, -2.0f, 1.0f},
    .rotation = math::quaternion{math::vector3::up, math::angle{math::radian{0.7f}}},
    .scale = math::vector3{2.0f, 1.0f, 0.5f}
  });

  auto child = scene.create_node("Child", scenes::local_transform{
    .position = math::vector3{0.5f, 4.0f, -1.0f},
    .rotation = math::quaternion{math::vector3::normalized(math::vector3{1.0f, 1.0f, 0.0f}), math::angle{math::radian{1.2f}}},
    .scale = math::vector3{1.0f, 3.0f, 2.0f}
  });

  child.set_parent(parent);
  scene.update();

  const auto& world = child.get_component<scenes::world_transform>();

  expect_near(world.matrix * world.inverse, math::matrix4x4::identity);
  expect_near(world.inverse, math::matrix4x4::inverted(world.matrix));
}
