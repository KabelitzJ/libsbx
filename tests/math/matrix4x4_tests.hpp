// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_TESTS_MATRIX4X4_TESTS_HPP_
#define LIBSBX_MATH_TESTS_MATRIX4X4_TESTS_HPP_

#include <gtest/gtest.h>

#include <libsbx/math/matrix4x4.hpp>

TEST(libsbx_math_matrix4x4, identity) {
  auto matrix = sbx::math::matrix4x4::identity;

  EXPECT_FLOAT_EQ(matrix[0][0], 1.0f);
  EXPECT_FLOAT_EQ(matrix[0][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[1][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][1], 1.0f);
  EXPECT_FLOAT_EQ(matrix[1][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[2][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][2], 1.0f);
  EXPECT_FLOAT_EQ(matrix[2][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[3][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][3], 1.0f);
}

TEST(libsbx_math_matrix4x4, zero) {
  auto matrix = sbx::math::matrix4x4::zero;

  EXPECT_FLOAT_EQ(matrix[0][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[1][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[2][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[3][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][3], 0.0f);
}

TEST(libsbx_math_matrix4x4, default_constructor) {
  auto matrix = sbx::math::matrix4x4{};

  EXPECT_FLOAT_EQ(matrix[0][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[1][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[2][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[3][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][3], 0.0f);
}

TEST(libsbx_math_matrix4x4, copy_constructor) {
  auto matrix = sbx::math::matrix4x4{
    1, 5, 9, 13,
    2, 6, 10, 14,
    3, 7, 11, 15,
    4, 8, 12, 16
  };

  auto copy = sbx::math::matrix4x4{matrix};

  EXPECT_FLOAT_EQ(copy[0][0], 1.0f);
  EXPECT_FLOAT_EQ(copy[0][1], 2.0f);
  EXPECT_FLOAT_EQ(copy[0][2], 3.0f);
  EXPECT_FLOAT_EQ(copy[0][3], 4.0f);

  EXPECT_FLOAT_EQ(copy[1][0], 5.0f);
  EXPECT_FLOAT_EQ(copy[1][1], 6.0f);
  EXPECT_FLOAT_EQ(copy[1][2], 7.0f);
  EXPECT_FLOAT_EQ(copy[1][3], 8.0f);

  EXPECT_FLOAT_EQ(copy[2][0], 9.0f);
  EXPECT_FLOAT_EQ(copy[2][1], 10.0f);
  EXPECT_FLOAT_EQ(copy[2][2], 11.0f);
  EXPECT_FLOAT_EQ(copy[2][3], 12.0f);

  EXPECT_FLOAT_EQ(copy[3][0], 13.0f);
  EXPECT_FLOAT_EQ(copy[3][1], 14.0f);
  EXPECT_FLOAT_EQ(copy[3][2], 15.0f);
  EXPECT_FLOAT_EQ(copy[3][3], 16.0f);
}

TEST(libsbx_math_matrix4x4, move_constructor) {
  auto matrix = sbx::math::matrix4x4{
    1, 5, 9, 13,
    2, 6, 10, 14,
    3, 7, 11, 15,
    4, 8, 12, 16
  };

  auto move = sbx::math::matrix4x4{std::move(matrix)};

  EXPECT_FLOAT_EQ(move[0][0], 1.0f);
  EXPECT_FLOAT_EQ(move[0][1], 2.0f);
  EXPECT_FLOAT_EQ(move[0][2], 3.0f);
  EXPECT_FLOAT_EQ(move[0][3], 4.0f);

  EXPECT_FLOAT_EQ(move[1][0], 5.0f);
  EXPECT_FLOAT_EQ(move[1][1], 6.0f);
  EXPECT_FLOAT_EQ(move[1][2], 7.0f);
  EXPECT_FLOAT_EQ(move[1][3], 8.0f);

  EXPECT_FLOAT_EQ(move[2][0], 9.0f);
  EXPECT_FLOAT_EQ(move[2][1], 10.0f);
  EXPECT_FLOAT_EQ(move[2][2], 11.0f);
  EXPECT_FLOAT_EQ(move[2][3], 12.0f);

  EXPECT_FLOAT_EQ(move[3][0], 13.0f);
  EXPECT_FLOAT_EQ(move[3][1], 14.0f);
  EXPECT_FLOAT_EQ(move[3][2], 15.0f);
  EXPECT_FLOAT_EQ(move[3][3], 16.0f);
}

TEST(libsbx_math_matrix4x4, scalar_constructor) {
  auto matrix = sbx::math::matrix4x4{1.0f};

  EXPECT_FLOAT_EQ(matrix[0][0], 1.0f);
  EXPECT_FLOAT_EQ(matrix[0][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[0][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[1][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][1], 1.0f);
  EXPECT_FLOAT_EQ(matrix[1][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[1][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[2][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[2][2], 1.0f);
  EXPECT_FLOAT_EQ(matrix[2][3], 0.0f);

  EXPECT_FLOAT_EQ(matrix[3][0], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][1], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][2], 0.0f);
  EXPECT_FLOAT_EQ(matrix[3][3], 1.0f);
}

TEST(libsbx_math_matrix4x4, copy_assignment) {
  auto matrix = sbx::math::matrix4x4{
    1, 5, 9, 13,
    2, 6, 10, 14,
    3, 7, 11, 15,
    4, 8, 12, 16
  };

  auto copy = matrix;

  EXPECT_FLOAT_EQ(copy[0][0], 1.0f);
  EXPECT_FLOAT_EQ(copy[0][1], 2.0f);
  EXPECT_FLOAT_EQ(copy[0][2], 3.0f);
  EXPECT_FLOAT_EQ(copy[0][3], 4.0f);

  EXPECT_FLOAT_EQ(copy[1][0], 5.0f);
  EXPECT_FLOAT_EQ(copy[1][1], 6.0f);
  EXPECT_FLOAT_EQ(copy[1][2], 7.0f);
  EXPECT_FLOAT_EQ(copy[1][3], 8.0f);

  EXPECT_FLOAT_EQ(copy[2][0], 9.0f);
  EXPECT_FLOAT_EQ(copy[2][1], 10.0f);
  EXPECT_FLOAT_EQ(copy[2][2], 11.0f);
  EXPECT_FLOAT_EQ(copy[2][3], 12.0f);

  EXPECT_FLOAT_EQ(copy[3][0], 13.0f);
  EXPECT_FLOAT_EQ(copy[3][1], 14.0f);
  EXPECT_FLOAT_EQ(copy[3][2], 15.0f);
  EXPECT_FLOAT_EQ(copy[3][3], 16.0f);
}

TEST(libsbx_math_matrix4x4, move_assignment) {
  auto matrix = sbx::math::matrix4x4{
    1, 5, 9, 13,
    2, 6, 10, 14,
    3, 7, 11, 15,
    4, 8, 12, 16
  };

  auto move = std::move(matrix);

  EXPECT_FLOAT_EQ(move[0][0], 1.0f);
  EXPECT_FLOAT_EQ(move[0][1], 2.0f);
  EXPECT_FLOAT_EQ(move[0][2], 3.0f);
  EXPECT_FLOAT_EQ(move[0][3], 4.0f);

  EXPECT_FLOAT_EQ(move[1][0], 5.0f);
  EXPECT_FLOAT_EQ(move[1][1], 6.0f);
  EXPECT_FLOAT_EQ(move[1][2], 7.0f);
  EXPECT_FLOAT_EQ(move[1][3], 8.0f);

  EXPECT_FLOAT_EQ(move[2][0], 9.0f);
  EXPECT_FLOAT_EQ(move[2][1], 10.0f);
  EXPECT_FLOAT_EQ(move[2][2], 11.0f);
  EXPECT_FLOAT_EQ(move[2][3], 12.0f);

  EXPECT_FLOAT_EQ(move[3][0], 13.0f);
  EXPECT_FLOAT_EQ(move[3][1], 14.0f);
  EXPECT_FLOAT_EQ(move[3][2], 15.0f);
  EXPECT_FLOAT_EQ(move[3][3], 16.0f);
}

TEST(libsbx_math_matrix4x4, transposed) {
  auto matrix = sbx::math::matrix4x4{
    1, 5, 9, 13,
    2, 6, 10, 14,
    3, 7, 11, 15,
    4, 8, 12, 16
  };

  auto result = sbx::math::matrix4x4::transposed(matrix);

  EXPECT_FLOAT_EQ(result[0][0], 1.0f);
  EXPECT_FLOAT_EQ(result[0][1], 5.0f);
  EXPECT_FLOAT_EQ(result[0][2], 9.0f);
  EXPECT_FLOAT_EQ(result[0][3], 13.0f);

  EXPECT_FLOAT_EQ(result[1][0], 2.0f);
  EXPECT_FLOAT_EQ(result[1][1], 6.0f);
  EXPECT_FLOAT_EQ(result[1][2], 10.0f);
  EXPECT_FLOAT_EQ(result[1][3], 14.0f);

  EXPECT_FLOAT_EQ(result[2][0], 3.0f);
  EXPECT_FLOAT_EQ(result[2][1], 7.0f);
  EXPECT_FLOAT_EQ(result[2][2], 11.0f);
  EXPECT_FLOAT_EQ(result[2][3], 15.0f);

  EXPECT_FLOAT_EQ(result[3][0], 4.0f);
  EXPECT_FLOAT_EQ(result[3][1], 8.0f);
  EXPECT_FLOAT_EQ(result[3][2], 12.0f);
  EXPECT_FLOAT_EQ(result[3][3], 16.0f);
}

TEST(libsbx_math_matrix4x4, translated) {
  auto matrix = sbx::math::matrix4x4{
    1, 5, 9, 13,
    2, 6, 10, 14,
    3, 7, 11, 15,
    4, 8, 12, 16
  };

  auto result = sbx::math::matrix4x4::translated(matrix, sbx::math::vector3{1.0f, 2.0f, 3.0f});

  EXPECT_FLOAT_EQ(result[0][0], 1.0f);
  EXPECT_FLOAT_EQ(result[0][1], 2.0f);
  EXPECT_FLOAT_EQ(result[0][2], 3.0f);
  EXPECT_FLOAT_EQ(result[0][3], 4.0f);

  EXPECT_FLOAT_EQ(result[1][0], 5.0f);
  EXPECT_FLOAT_EQ(result[1][1], 6.0f);
  EXPECT_FLOAT_EQ(result[1][2], 7.0f);
  EXPECT_FLOAT_EQ(result[1][3], 8.0f);

  EXPECT_FLOAT_EQ(result[2][0], 9.0f);
  EXPECT_FLOAT_EQ(result[2][1], 10.0f);
  EXPECT_FLOAT_EQ(result[2][2], 11.0f);
  EXPECT_FLOAT_EQ(result[2][3], 12.0f);

  EXPECT_FLOAT_EQ(result[3][0], 51.0f);
  EXPECT_FLOAT_EQ(result[3][1], 58.0f);
  EXPECT_FLOAT_EQ(result[3][2], 65.0f);
  EXPECT_FLOAT_EQ(result[3][3], 72.0f);
}

#endif // LIBSBX_MATH_TESTS_MATRIX4X4_TESTS_HPP_
