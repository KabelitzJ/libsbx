// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <vector>

#include <gtest/gtest.h>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene.hpp>

namespace scenes = sbx::scenes;
namespace math = sbx::math;

TEST(scene_destroy, destroys_a_deep_subtree_and_nothing_else) {
  auto scene = scenes::scene{"Test"};

  auto keep = scene.create_node("Keep");
  auto top = scene.create_node("Top");

  auto ids = std::vector<math::uuid>{top.id()};
  auto parent = top;

  // A chain with a sibling at every level: depth and breadth both.
  for (auto depth = 0; depth < 6; ++depth) {
    auto child = scene.create_node("Child");
    auto sibling = scene.create_node("Sibling");

    child.set_parent(parent);
    sibling.set_parent(parent);

    ids.push_back(child.id());
    ids.push_back(sibling.id());

    parent = child;
  }

  scene.destroy_node(top);

  for (const auto& id : ids) {
    EXPECT_FALSE(scene.find(id).is_valid());
  }

  EXPECT_TRUE(scene.find(keep.id()).is_valid());
  EXPECT_EQ(scene.root().get_component<scenes::relationship>().children.size(), 1u);
}
