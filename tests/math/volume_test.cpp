// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/math/volume.hpp>
#include <libsbx/math/ray.hpp>

using namespace sbx::math;

TEST(volume_test, contains_point_inside_and_rejects_point_outside) {
  const auto box = volume{vector3{-1.0f, -1.0f, -1.0f}, vector3{1.0f, 1.0f, 1.0f}};

  EXPECT_TRUE(box.contains(vector3::zero));
  EXPECT_TRUE(box.contains(vector3{1.0f, 1.0f, 1.0f})); // On the boundary counts as contained.
  EXPECT_FALSE(box.contains(vector3{2.0f, 0.0f, 0.0f}));
}

TEST(volume_test, contains_volume_when_fully_enclosed) {
  const auto outer = volume{vector3{-2.0f, -2.0f, -2.0f}, vector3{2.0f, 2.0f, 2.0f}};
  const auto inner = volume{vector3{-1.0f, -1.0f, -1.0f}, vector3{1.0f, 1.0f, 1.0f}};

  EXPECT_TRUE(outer.contains(inner));
  EXPECT_FALSE(inner.contains(outer));
}

TEST(volume_test, intersects_overlapping_volumes_but_not_disjoint_ones) {
  const auto a = volume{vector3{0.0f, 0.0f, 0.0f}, vector3{2.0f, 2.0f, 2.0f}};
  const auto overlapping = volume{vector3{1.0f, 1.0f, 1.0f}, vector3{3.0f, 3.0f, 3.0f}};
  const auto disjoint = volume{vector3{5.0f, 5.0f, 5.0f}, vector3{6.0f, 6.0f, 6.0f}};

  EXPECT_TRUE(a.intersects(overlapping));
  EXPECT_FALSE(a.intersects(disjoint));
}

TEST(volume_test, extend_and_diagonal_length_match_the_min_and_max_corners) {
  const auto box = volume{vector3{0.0f, 0.0f, 0.0f}, vector3{3.0f, 4.0f, 0.0f}};

  const auto extend = box.extend();
  EXPECT_FLOAT_EQ(extend.x(), 3.0f);
  EXPECT_FLOAT_EQ(extend.y(), 4.0f);

  EXPECT_NEAR(box.diagonal_length(), 5.0f, 1e-4f);
}

TEST(volume_test, center_is_the_midpoint_of_min_and_max) {
  const auto box = volume{vector3{-2.0f, -2.0f, -2.0f}, vector3{4.0f, 4.0f, 4.0f}};

  const auto center = box.center();

  EXPECT_FLOAT_EQ(center.x(), 1.0f);
  EXPECT_FLOAT_EQ(center.y(), 1.0f);
  EXPECT_FLOAT_EQ(center.z(), 1.0f);
}

TEST(volume_test, ray_intersects_a_box_it_points_at) {
  const auto box = volume{vector3{-1.0f, -1.0f, -1.0f}, vector3{1.0f, 1.0f, 1.0f}};
  const auto ray = sbx::math::ray{vector3{-5.0f, 0.0f, 0.0f}, vector3::right};

  const auto hit = box.intersects(ray);

  ASSERT_TRUE(hit.has_value());
  EXPECT_NEAR(*hit, 4.0f, 1e-4f); // Distance from origin (-5, 0, 0) to the near face at x = -1.
}

TEST(volume_test, ray_misses_a_box_it_does_not_point_at) {
  const auto box = volume{vector3{-1.0f, -1.0f, -1.0f}, vector3{1.0f, 1.0f, 1.0f}};
  const auto ray = sbx::math::ray{vector3{-5.0f, 5.0f, 0.0f}, vector3::right};

  EXPECT_FALSE(box.intersects(ray).has_value());
}

TEST(volume_test, ray_pointing_away_from_a_box_does_not_intersect) {
  const auto box = volume{vector3{-1.0f, -1.0f, -1.0f}, vector3{1.0f, 1.0f, 1.0f}};
  const auto ray = sbx::math::ray{vector3{-5.0f, 0.0f, 0.0f}, vector3::left};

  EXPECT_FALSE(box.intersects(ray).has_value());
}
