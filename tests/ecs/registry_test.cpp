// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/ecs/registry.hpp>

using namespace sbx::ecs;

struct position {
  std::float_t x{};
  std::float_t y{};
};

struct velocity {
  std::float_t dx{};
  std::float_t dy{};
};

TEST(registry_test, create_returns_distinct_valid_entities) {
  auto registry = sbx::ecs::registry{};

  const auto a = registry.create();
  const auto b = registry.create();

  EXPECT_NE(a, b);
  EXPECT_TRUE(registry.is_valid(a));
  EXPECT_TRUE(registry.is_valid(b));
}

TEST(registry_test, destroy_invalidates_the_entity) {
  auto registry = sbx::ecs::registry{};

  const auto entity = registry.create();
  registry.destroy(entity);

  EXPECT_FALSE(registry.is_valid(entity));
}

TEST(registry_test, emplace_and_get_round_trip_a_component) {
  auto registry = sbx::ecs::registry{};

  const auto entity = registry.create();
  registry.emplace<position>(entity, 1.0f, 2.0f);

  const auto& component = registry.get<position>(entity);

  EXPECT_FLOAT_EQ(component.x, 1.0f);
  EXPECT_FLOAT_EQ(component.y, 2.0f);
}

TEST(registry_test, all_of_and_any_of_reflect_which_components_are_present) {
  auto registry = sbx::ecs::registry{};

  const auto entity = registry.create();
  registry.emplace<position>(entity, 0.0f, 0.0f);

  EXPECT_TRUE((registry.all_of<position>(entity)));
  EXPECT_FALSE((registry.all_of<velocity>(entity)));
  EXPECT_FALSE((registry.all_of<position, velocity>(entity)));
  EXPECT_TRUE((registry.any_of<position, velocity>(entity)));
}

TEST(registry_test, try_get_returns_null_for_a_missing_component) {
  auto registry = sbx::ecs::registry{};

  const auto entity = registry.create();
  registry.emplace<position>(entity, 0.0f, 0.0f);

  EXPECT_NE(registry.try_get<position>(entity), nullptr);
  EXPECT_EQ(registry.try_get<velocity>(entity), nullptr);
}

TEST(registry_test, remove_drops_the_component_but_keeps_the_entity_valid) {
  auto registry = sbx::ecs::registry{};

  const auto entity = registry.create();
  registry.emplace<position>(entity, 0.0f, 0.0f);

  registry.remove<position>(entity);

  EXPECT_TRUE(registry.is_valid(entity));
  EXPECT_FALSE((registry.all_of<position>(entity)));
}

TEST(registry_test, destroying_an_entity_removes_all_of_its_components) {
  auto registry = sbx::ecs::registry{};

  const auto entity = registry.create();
  registry.emplace<position>(entity, 1.0f, 1.0f);
  registry.emplace<velocity>(entity, 2.0f, 2.0f);

  registry.destroy(entity);
  const auto recreated = registry.create();

  // A freshly created entity should not carry over components from a destroyed one,
  // even if the underlying id slot gets recycled.
  EXPECT_FALSE((registry.all_of<position>(recreated)));
  EXPECT_FALSE((registry.all_of<velocity>(recreated)));
}

TEST(registry_test, view_iterates_only_entities_that_have_the_requested_component) {
  auto registry = sbx::ecs::registry{};

  const auto with_position = registry.create();
  registry.emplace<position>(with_position, 3.0f, 4.0f);

  const auto without_position = registry.create();
  registry.emplace<velocity>(without_position, 1.0f, 1.0f);

  auto count = std::size_t{0};
  for (const auto entity : registry.view<position>()) {
    EXPECT_EQ(entity, with_position);
    ++count;
  }

  EXPECT_EQ(count, 1u);
}

TEST(registry_test, view_with_two_components_only_matches_entities_with_both) {
  auto registry = sbx::ecs::registry{};

  const auto both = registry.create();
  registry.emplace<position>(both, 0.0f, 0.0f);
  registry.emplace<velocity>(both, 1.0f, 1.0f);

  const auto only_position = registry.create();
  registry.emplace<position>(only_position, 0.0f, 0.0f);

  auto count = std::size_t{0};
  for (const auto entity : registry.view<position, velocity>()) {
    EXPECT_EQ(entity, both);
    ++count;
  }

  EXPECT_EQ(count, 1u);
}

TEST(registry_test, clear_removes_every_entity) {
  auto registry = sbx::ecs::registry{};

  const auto a = registry.create();
  const auto b = registry.create();
  registry.emplace<position>(a, 0.0f, 0.0f);

  registry.clear();

  EXPECT_FALSE(registry.is_valid(a));
  EXPECT_FALSE(registry.is_valid(b));
}
