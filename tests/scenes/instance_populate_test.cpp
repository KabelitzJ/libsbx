// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <array>
#include <cmath>
#include <cstring>

#include <gtest/gtest.h>

#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/instance_populate.hpp>

namespace scenes = sbx::scenes;
namespace math = sbx::math;

// A 10 × 10 floor at y = 2, as two triangles facing up.
auto floor_triangles() -> std::array<scenes::populate_triangle, 2u> {
  return {
    scenes::populate_triangle{math::vector3{0.0f, 2.0f, 0.0f}, math::vector3{10.0f, 2.0f, 0.0f}, math::vector3{10.0f, 2.0f, 10.0f}, math::vector3{0.0f, 1.0f, 0.0f}},
    scenes::populate_triangle{math::vector3{0.0f, 2.0f, 0.0f}, math::vector3{10.0f, 2.0f, 10.0f}, math::vector3{0.0f, 2.0f, 10.0f}, math::vector3{0.0f, 1.0f, 0.0f}}
  };
}

auto column_length(const scenes::instance_data& instance, std::size_t column) -> std::float_t {
  const auto x = instance.rows[0][column];
  const auto y = instance.rows[1][column];
  const auto z = instance.rows[2][column];

  return std::sqrt(x * x + y * y + z * z);
}

TEST(instance_populate, places_the_requested_amount_on_the_surface) {
  const auto triangles = floor_triangles();

  const auto instances = scenes::populate_surface(triangles, scenes::populate_options{.amount = 500u, .random_rotation = 180.0f, .seed = 7u});

  ASSERT_EQ(instances.size(), 500u);

  for (const auto& instance : instances) {
    EXPECT_NEAR(instance.rows[1][3], 2.0f, 1e-4f);
    EXPECT_GE(instance.rows[0][3], -1e-4f);
    EXPECT_LE(instance.rows[0][3], 10.0f + 1e-4f);
    EXPECT_GE(instance.rows[2][3], -1e-4f);
    EXPECT_LE(instance.rows[2][3], 10.0f + 1e-4f);
  }
}

TEST(instance_populate, the_same_seed_gives_the_same_instances) {
  const auto triangles = floor_triangles();
  const auto options = scenes::populate_options{.amount = 64u, .random_rotation = 90.0f, .random_tilt = 15.0f, .random_scale = 0.3f, .seed = 42u};

  const auto first = scenes::populate_surface(triangles, options);
  const auto second = scenes::populate_surface(triangles, options);

  ASSERT_EQ(first.size(), second.size());
  EXPECT_EQ(std::memcmp(first.data(), second.data(), first.size() * sizeof(scenes::instance_data)), 0);
}

TEST(instance_populate, without_randomness_instances_stand_upright_at_the_given_scale) {
  const auto triangles = floor_triangles();

  const auto instances = scenes::populate_surface(triangles, scenes::populate_options{.amount = 16u, .scale = 2.0f, .seed = 1u});

  for (const auto& instance : instances) {
    EXPECT_NEAR(instance.rows[0][0], 2.0f, 1e-4f);
    EXPECT_NEAR(instance.rows[1][1], 2.0f, 1e-4f);
    EXPECT_NEAR(instance.rows[2][2], 2.0f, 1e-4f);
    EXPECT_NEAR(instance.rows[0][1], 0.0f, 1e-4f);
  }
}

TEST(instance_populate, the_up_axis_follows_the_surface_normal) {
  // A wall facing +X; the source's +Z is its up.
  const auto wall = std::array<scenes::populate_triangle, 1u>{
    scenes::populate_triangle{math::vector3{0.0f, 0.0f, 0.0f}, math::vector3{0.0f, 5.0f, 0.0f}, math::vector3{0.0f, 0.0f, 5.0f}, math::vector3{1.0f, 0.0f, 0.0f}}
  };

  const auto instances = scenes::populate_surface(wall, scenes::populate_options{.amount = 8u, .mesh_up_axis = scenes::populate_up_axis::z, .seed = 3u});

  for (const auto& instance : instances) {
    // The rotation's third column (where local +Z goes) points along +X.
    EXPECT_NEAR(instance.rows[0][2], 1.0f, 1e-4f);
    EXPECT_NEAR(instance.rows[1][2], 0.0f, 1e-4f);
    EXPECT_NEAR(instance.rows[2][2], 0.0f, 1e-4f);
  }
}

TEST(instance_populate, random_scale_stays_in_range) {
  const auto triangles = floor_triangles();

  const auto instances = scenes::populate_surface(triangles, scenes::populate_options{.amount = 200u, .random_rotation = 180.0f, .random_tilt = 30.0f, .scale = 1.5f, .random_scale = 0.2f, .seed = 9u});

  for (const auto& instance : instances) {
    const auto scale = column_length(instance, 0u);

    EXPECT_GE(scale, 1.5f * 0.8f - 1e-4f);
    EXPECT_LE(scale, 1.5f * 1.2f + 1e-4f);
    EXPECT_NEAR(column_length(instance, 1u), scale, 1e-3f);
  }
}

TEST(instance_populate, a_surface_without_area_places_nothing) {
  const auto degenerate = std::array<scenes::populate_triangle, 1u>{
    scenes::populate_triangle{math::vector3{0.0f, 0.0f, 0.0f}, math::vector3{1.0f, 0.0f, 0.0f}, math::vector3{2.0f, 0.0f, 0.0f}}
  };

  EXPECT_TRUE(scenes::populate_surface(degenerate, scenes::populate_options{}).empty());
}
