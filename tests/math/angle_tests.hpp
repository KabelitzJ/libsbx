// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_TESTS_ANGLE_TESTS_HPP_
#define LIBSBX_MATH_TESTS_ANGLE_TESTS_HPP_

#include <gtest/gtest.h>

#include <libsbx/math/angle.hpp>

TEST(libsbx_math_angle, degree) {
  using namespace sbx::math::literals;

  auto degree = 90_deg;

  EXPECT_FLOAT_EQ(degree.value(), 90.0f);
}

TEST(libsbx_math_angle, radian) {
  using namespace sbx::math::literals;

  auto radian = 0.25_rad;

  EXPECT_FLOAT_EQ(radian.value(), 0.25f);
}

TEST(libsbx_math_angle, degree_to_radian) {
  using namespace sbx::math::literals;

  auto degree = 90_deg;
  auto radian = sbx::math::to_radians(degree);

  EXPECT_FLOAT_EQ(radian.value(), std::numbers::pi_v<float> / 2.0f);
}

TEST(libsbx_math_angle, radian_to_degree) {
  using namespace sbx::math::literals;

  auto radian = 1.57079632679_rad;
  auto degree = sbx::math::to_degrees(radian);

  EXPECT_FLOAT_EQ(degree.value(), 90.0f);
}

#endif // LIBSBX_MATH_TESTS_ANGLE_TESTS_HPP_
