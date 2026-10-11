// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <cmath>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <libsbx/math/ray.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/terrain/heightmap.hpp>

#include <libsbx/physics/gjk.hpp>
#include <libsbx/physics/raycast.hpp>
#include <libsbx/physics/shape_cast.hpp>

using namespace sbx;
using namespace sbx::physics;

constexpr auto tolerance = 2e-3f;

static auto at(const math::vector3& position) -> transform {
  return transform{position, math::quaternion::identity, math::vector3::one};
}

TEST(physics_queries_test, gjk_distance_between_separated_shapes) {
  const auto spheres = gjk_distance(sphere{1.0f}, at({0.0f, 0.0f, 0.0f}), sphere{0.5f}, at({4.0f, 0.0f, 0.0f}));

  ASSERT_FALSE(spheres.intersecting);
  EXPECT_NEAR(spheres.distance, 2.5f, tolerance);
  EXPECT_NEAR(spheres.normal.x(), -1.0f, tolerance); // from B toward A
  EXPECT_NEAR(spheres.point_on_a.x(), 1.0f, tolerance);
  EXPECT_NEAR(spheres.point_on_b.x(), 3.5f, tolerance);

  // Box corner region: closest points are the facing faces 1 m apart along Y.
  const auto boxes = gjk_distance(box{{0.5f, 0.5f, 0.5f}}, at({0.0f, 0.0f, 0.0f}), box{{0.5f, 0.5f, 0.5f}}, at({0.2f, 2.0f, -0.3f}));

  ASSERT_FALSE(boxes.intersecting);
  EXPECT_NEAR(boxes.distance, 1.0f, tolerance);

  EXPECT_TRUE(gjk_distance(sphere{1.0f}, at({0.0f, 0.0f, 0.0f}), box{{0.5f, 0.5f, 0.5f}}, at({1.2f, 0.0f, 0.0f})).intersecting);
}

TEST(physics_queries_test, cast_convex_reports_time_of_impact) {
  // A 0.5 m sphere moving +X toward a box whose near face is at x = 4.5: it touches after 4 m.
  const auto hit = cast_convex(sphere{0.5f}, at({0.0f, 0.0f, 0.0f}), math::vector3{1.0f, 0.0f, 0.0f}, 10.0f, box{{0.5f, 2.0f, 2.0f}}, at({5.0f, 0.0f, 0.0f}));

  ASSERT_TRUE(hit.has_value());
  EXPECT_FALSE(hit->initially_overlapping);
  EXPECT_NEAR(hit->distance, 4.0f, tolerance);
  EXPECT_NEAR(hit->normal.x(), -1.0f, tolerance);

  EXPECT_FALSE(cast_convex(sphere{0.5f}, at({0.0f, 0.0f, 0.0f}), math::vector3{1.0f, 0.0f, 0.0f}, 3.0f, box{{0.5f, 2.0f, 2.0f}}, at({5.0f, 0.0f, 0.0f})).has_value());
  EXPECT_FALSE(cast_convex(sphere{0.5f}, at({0.0f, 0.0f, 0.0f}), math::vector3{-1.0f, 0.0f, 0.0f}, 10.0f, box{{0.5f, 2.0f, 2.0f}}, at({5.0f, 0.0f, 0.0f})).has_value());

  const auto overlapping = cast_convex(sphere{0.5f}, at({4.8f, 0.0f, 0.0f}), math::vector3{1.0f, 0.0f, 0.0f}, 10.0f, box{{0.5f, 2.0f, 2.0f}}, at({5.0f, 0.0f, 0.0f}));

  ASSERT_TRUE(overlapping.has_value());
  EXPECT_TRUE(overlapping->initially_overlapping);
}

TEST(physics_queries_test, cast_against_flat_heightfield) {
  // 5x5 samples, 1 m cells, all at height 2: a capsule dropped from y = 10 lands with its bottom on y = 2.
  const auto map = terrain::heightmap{5u, 5u, 1.0f, std::vector<std::float_t>(25u, 2.0f)};

  const auto hit = cast_against_heightfield(capsule{0.5f, 0.5f}, at({0.3f, 10.0f, -0.4f}), math::vector3{0.0f, -1.0f, 0.0f}, 20.0f, map);

  ASSERT_TRUE(hit.has_value());
  EXPECT_NEAR(hit->distance, 7.0f, tolerance); // bottom starts at 10 - 1 = 9
  EXPECT_NEAR(std::abs(hit->normal.y()), 1.0f, tolerance);
}

TEST(physics_queries_test, raycast_hits_triangle_mesh) {
  // One quad in the XZ plane at y = 0, two triangles.
  auto mesh = mesh_collision_data{};
  mesh.vertices = {{-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}};
  mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u};
  mesh.triangle_bvh.insert(0u, math::volume{{-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 1.0f}});
  mesh.triangle_bvh.insert(1u, math::volume{{-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 1.0f}});

  // Scaled by 2 and raised by 3: the quad spans x, z in [-2, 2] at y = 3.
  const auto pose = transform{{0.0f, 3.0f, 0.0f}, math::quaternion::identity, {2.0f, 2.0f, 2.0f}};

  const auto hit = raycast_triangle_mesh(mesh, pose, math::ray{{1.5f, 10.0f, 1.5f}, {0.0f, -1.0f, 0.0f}}, 100.0f);

  ASSERT_TRUE(hit.has_value());
  EXPECT_NEAR(hit->distance, 7.0f, tolerance);
  EXPECT_NEAR(hit->normal.y(), 1.0f, tolerance); // faces the ray's origin

  EXPECT_FALSE(raycast_triangle_mesh(mesh, pose, math::ray{{2.5f, 10.0f, 0.0f}, {0.0f, -1.0f, 0.0f}}, 100.0f).has_value());
}
