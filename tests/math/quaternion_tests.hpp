// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_TESTS_QUATERNION_TESTS_HPP_
#define LIBSBX_MATH_TESTS_QUATERNION_TESTS_HPP_

#include <gtest/gtest.h>

#include <libsbx/math/quaternion.hpp>

TEST(libsbx_math_quaternion, default_constructor) {
  auto quaternion = sbx::math::quaternion{};

  EXPECT_FLOAT_EQ(quaternion.complex().x(), 0.0f);
  EXPECT_FLOAT_EQ(quaternion.complex().y(), 0.0f);
  EXPECT_FLOAT_EQ(quaternion.complex().z(), 0.0f);
  EXPECT_FLOAT_EQ(quaternion.scalar(), 0.0f);
}

TEST(libsbx_math_quaternion, constructor) {
  auto quaternion = sbx::math::quaternion{1.0f, 2.0f, 3.0f, 4.0f};

  EXPECT_FLOAT_EQ(quaternion.complex().x(), 1.0f);
  EXPECT_FLOAT_EQ(quaternion.complex().y(), 2.0f);
  EXPECT_FLOAT_EQ(quaternion.complex().z(), 3.0f);
  EXPECT_FLOAT_EQ(quaternion.scalar(), 4.0f);
}

TEST(libsbx_math_quaternion, vector_constructor) {
  auto quaternion = sbx::math::quaternion{sbx::math::vector3{1.0f, 2.0f, 3.0f}, 4.0f};

  EXPECT_FLOAT_EQ(quaternion.complex().x(), 1.0f);
  EXPECT_FLOAT_EQ(quaternion.complex().y(), 2.0f);
  EXPECT_FLOAT_EQ(quaternion.complex().z(), 3.0f);
  EXPECT_FLOAT_EQ(quaternion.scalar(), 4.0f);
}

TEST(libsbx_math_quaternion, equality) {
  auto quaternion = sbx::math::quaternion{1.0f, 2.0f, 3.0f, 4.0f};

  auto other = sbx::math::quaternion{1.0f, 2.0f, 3.0f, 4.0f};

  EXPECT_EQ(quaternion, other);
}

TEST(libsbx_math_quaternion, inequality) {
  auto quaternion = sbx::math::quaternion{1.0f, 2.0f, 3.0f, 4.0f};

  auto other = sbx::math::quaternion{5.0f, 6.0f, 7.0f, 8.0f};

  EXPECT_NE(quaternion, other);
}

#endif // LIBSBX_MATH_TESTS_QUATERNION_TESTS_HPP_
