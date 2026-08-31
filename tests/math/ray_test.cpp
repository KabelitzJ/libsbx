// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/math/ray.hpp>
#include <libsbx/math/plane.hpp>

using namespace sbx::math;

TEST(ray_test, default_ray_originates_at_zero_and_points_forward) {
  const auto ray = sbx::math::ray{};

  EXPECT_EQ(ray.origin(), vector3::zero);
  EXPECT_EQ(ray.direction(), vector3::forward);
}

TEST(ray_test, direction_is_normalized_on_construction) {
  const auto ray = sbx::math::ray{vector3::zero, vector3{5.0f, 0.0f, 0.0f}};

  EXPECT_NEAR(ray.direction().length(), 1.0f, 1e-4f);
  EXPECT_NEAR(ray.direction().x(), 1.0f, 1e-4f);
}

TEST(ray_test, point_at_computes_origin_plus_direction_times_t) {
  const auto ray = sbx::math::ray{vector3::zero, vector3::right};

  const auto point = ray.point_at(3.0f);

  EXPECT_NEAR(point.x(), 3.0f, 1e-4f);
  EXPECT_NEAR(point.y(), 0.0f, 1e-4f);
  EXPECT_NEAR(point.z(), 0.0f, 1e-4f);
}

TEST(ray_test, ray_intersects_a_plane_it_points_at) {
  // Horizontal plane at y = 5.
  auto surface = plane{vector3::up, -5.0f};

  const auto ray = sbx::math::ray{vector3::zero, vector3::up};

  const auto hit = surface.ray_intersect(ray);

  ASSERT_TRUE(hit.has_value());
  EXPECT_NEAR(hit->x(), 0.0f, 1e-4f);
  EXPECT_NEAR(hit->y(), 5.0f, 1e-4f);
  EXPECT_NEAR(hit->z(), 0.0f, 1e-4f);
}

TEST(ray_test, ray_parallel_to_a_plane_does_not_intersect) {
  auto surface = plane{vector3::up, -5.0f};

  const auto ray = sbx::math::ray{vector3::zero, vector3::right};

  EXPECT_FALSE(surface.ray_intersect(ray).has_value());
}

TEST(ray_test, ray_pointing_away_from_a_plane_does_not_intersect) {
  auto surface = plane{vector3::up, -5.0f};

  const auto ray = sbx::math::ray{vector3::zero, vector3::down};

  EXPECT_FALSE(surface.ray_intersect(ray).has_value());
}
