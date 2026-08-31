// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <cmath>

#include <gtest/gtest.h>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/angle.hpp>

using namespace sbx::math;

namespace {

auto expect_quaternion_near(const quaternion& lhs, const quaternion& rhs, std::float_t epsilon = 1e-3f) -> void {
  EXPECT_NEAR(lhs.x(), rhs.x(), epsilon);
  EXPECT_NEAR(lhs.y(), rhs.y(), epsilon);
  EXPECT_NEAR(lhs.z(), rhs.z(), epsilon);
  EXPECT_NEAR(lhs.w(), rhs.w(), epsilon);
}

auto expect_vector3_near(const vector3& lhs, const vector3& rhs, std::float_t epsilon = 1e-3f) -> void {
  EXPECT_NEAR(lhs.x(), rhs.x(), epsilon);
  EXPECT_NEAR(lhs.y(), rhs.y(), epsilon);
  EXPECT_NEAR(lhs.z(), rhs.z(), epsilon);
}

} // namespace

TEST(quaternion_test, identity_does_not_rotate_a_vector) {
  const auto rotated = quaternion::identity * vector3::right;

  expect_vector3_near(rotated, vector3::right);
}

TEST(quaternion_test, normalized_produces_unit_length_quaternion) {
  const auto quat = quaternion{1.0f, 2.0f, 3.0f, 4.0f};
  const auto normalized = quaternion::normalized(quat);

  EXPECT_NEAR(normalized.length(), 1.0f, 1e-4f);
}

TEST(quaternion_test, axis_angle_constructor_matches_half_angle_formula) {
  const auto rotation_angle = angle{degree{90.0f}};
  const auto quat = quaternion{vector3::up, rotation_angle};

  const auto half_angle_radians = rotation_angle.to_radians().value() / 2.0f;

  EXPECT_NEAR(quat.w(), std::cos(half_angle_radians), 1e-4f);
  EXPECT_NEAR(quat.complex().length(), std::sin(half_angle_radians), 1e-4f);
}

TEST(quaternion_test, conjugate_of_a_unit_quaternion_is_its_inverse) {
  const auto quat = quaternion{vector3::up, angle{degree{57.0f}}};

  expect_quaternion_near(quaternion::conjugate(quat), quaternion::inverted(quat));
}

TEST(quaternion_test, quaternion_times_its_inverse_is_identity) {
  // Scaled so it is not already unit length, exercising the general (non-unit) inverse path.
  const auto quat = quaternion{vector3::up, angle{degree{123.0f}}} * 2.0f;

  const auto product = quat * quaternion::inverted(quat);

  expect_quaternion_near(product, quaternion::identity);
}

TEST(quaternion_test, rotation_matches_matrix4x4_rotated_for_the_same_axis_and_angle) {
  const auto axis = vector3::up;
  const auto rotation_angle = angle{degree{90.0f}};

  const auto quat = quaternion{axis, rotation_angle};
  const auto rotated_by_quaternion = quat * vector3::right;

  const auto rotation_matrix = matrix4x4::rotated(matrix4x4::identity, axis, rotation_angle);
  const auto rotated_by_matrix = static_cast<vector3>(rotation_matrix * vector4{vector3::right, 0.0f});

  expect_vector3_near(rotated_by_quaternion, rotated_by_matrix);
}

TEST(quaternion_test, slerp_at_the_endpoints_returns_the_original_quaternions) {
  const auto start = quaternion::identity;
  const auto end = quaternion{vector3::up, angle{degree{90.0f}}};

  expect_quaternion_near(quaternion::slerp(start, end, 0.0f), start);
  expect_quaternion_near(quaternion::slerp(start, end, 1.0f), end);
}

TEST(quaternion_test, dot_product_of_a_unit_quaternion_with_itself_is_one) {
  const auto quat = quaternion{vector3::up, angle{degree{40.0f}}};

  EXPECT_NEAR(quaternion::dot(quat, quat), 1.0f, 1e-4f);
}
