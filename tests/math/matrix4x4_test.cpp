// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <cstddef>

#include <gtest/gtest.h>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/angle.hpp>

using namespace sbx::math;

namespace {

auto expect_matrix_near(const matrix4x4& lhs, const matrix4x4& rhs, std::float_t epsilon = 1e-4f) -> void {
  for (auto column = std::size_t{0u}; column < 4u; ++column) {
    for (auto row = std::size_t{0u}; row < 4u; ++row) {
      EXPECT_NEAR(lhs[column][row], rhs[column][row], epsilon) << "at column " << column << ", row " << row;
    }
  }
}

} // namespace

TEST(matrix4x4_test, identity_leaves_a_point_unchanged) {
  const auto point = vector4{1.0f, 2.0f, 3.0f, 1.0f};

  const auto result = matrix4x4::identity * point;

  EXPECT_FLOAT_EQ(result.x(), point.x());
  EXPECT_FLOAT_EQ(result.y(), point.y());
  EXPECT_FLOAT_EQ(result.z(), point.z());
  EXPECT_FLOAT_EQ(result.w(), point.w());
}

TEST(matrix4x4_test, transpose_swaps_rows_and_columns) {
  const auto matrix = matrix4x4{
    1.0f, 2.0f, 3.0f, 4.0f,
    5.0f, 6.0f, 7.0f, 8.0f,
    9.0f, 10.0f, 11.0f, 12.0f,
    13.0f, 14.0f, 15.0f, 16.0f
  };

  const auto transposed = matrix4x4::transposed(matrix);

  EXPECT_FLOAT_EQ(transposed[0][1], 2.0f);
  EXPECT_FLOAT_EQ(transposed[1][0], 5.0f);
  EXPECT_FLOAT_EQ(transposed[2][3], 12.0f);
  EXPECT_FLOAT_EQ(transposed[3][2], 15.0f);
}

TEST(matrix4x4_test, transpose_is_its_own_inverse_operation) {
  const auto matrix = matrix4x4{
    1.0f, 2.0f, 3.0f, 4.0f,
    5.0f, 6.0f, 7.0f, 8.0f,
    9.0f, 10.0f, 11.0f, 12.0f,
    13.0f, 14.0f, 15.0f, 16.0f
  };

  const auto transposed_twice = matrix4x4::transposed(matrix4x4::transposed(matrix));

  expect_matrix_near(transposed_twice, matrix);
}

TEST(matrix4x4_test, inverted_identity_is_identity) {
  const auto inverted = matrix4x4::inverted(matrix4x4::identity);

  expect_matrix_near(inverted, matrix4x4::identity);
}

TEST(matrix4x4_test, matrix_times_its_inverse_is_identity) {
  auto matrix = matrix4x4::identity;
  matrix = matrix4x4::translated(matrix, vector3{5.0f, -3.0f, 2.0f});
  matrix = matrix4x4::rotated(matrix, vector3::up, angle{degree{45.0f}});
  matrix = matrix4x4::scaled(matrix, vector3{2.0f, 0.5f, 1.5f});

  const auto inverted = matrix4x4::inverted(matrix);
  const auto product = matrix * inverted;

  expect_matrix_near(product, matrix4x4::identity, 1e-3f);
}

TEST(matrix4x4_test, translated_moves_a_point_by_the_given_offset) {
  const auto translation = matrix4x4::translated(matrix4x4::identity, vector3{1.0f, 2.0f, 3.0f});

  const auto moved = translation * vector4{0.0f, 0.0f, 0.0f, 1.0f};

  EXPECT_FLOAT_EQ(moved.x(), 1.0f);
  EXPECT_FLOAT_EQ(moved.y(), 2.0f);
  EXPECT_FLOAT_EQ(moved.z(), 3.0f);
}

TEST(matrix4x4_test, scaled_multiplies_the_relevant_axes) {
  const auto scale = matrix4x4::scaled(matrix4x4::identity, vector3{2.0f, 3.0f, 4.0f});

  const auto scaled = scale * vector4{1.0f, 1.0f, 1.0f, 1.0f};

  EXPECT_FLOAT_EQ(scaled.x(), 2.0f);
  EXPECT_FLOAT_EQ(scaled.y(), 3.0f);
  EXPECT_FLOAT_EQ(scaled.z(), 4.0f);
}

TEST(matrix4x4_test, rotated_by_a_full_turn_returns_to_the_identity_rotation) {
  const auto rotated = matrix4x4::rotated(matrix4x4::identity, vector3::up, angle{degree{360.0f}});

  expect_matrix_near(rotated, matrix4x4::identity, 1e-3f);
}
