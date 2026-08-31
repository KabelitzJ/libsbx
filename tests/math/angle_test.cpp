// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/constants.hpp>

using namespace sbx::math;
using namespace sbx::math::literals;

TEST(angle_test, degree_to_radian_conversion_matches_known_values) {
  EXPECT_NEAR(to_radians(degree{180.0f}).value(), pi_v<std::float_t>, 1e-4f);
  EXPECT_NEAR(to_radians(degree{360.0f}).value(), two_pi_v<std::float_t>, 1e-4f);
  EXPECT_NEAR(to_degrees(radian{pi_v<std::float_t>}).value(), 180.0f, 1e-3f);
}

TEST(angle_test, angle_constructed_from_degree_and_radian_agree) {
  const auto from_degree = angle{degree{180.0f}};
  const auto from_radian = angle{radian{pi_v<std::float_t>}};

  EXPECT_NEAR(from_degree.to_radians().value(), from_radian.to_radians().value(), 1e-4f);
}

TEST(angle_test, trigonometric_functions_match_known_values) {
  EXPECT_NEAR(sin(degree{90.0f}), 1.0f, 1e-4f);
  EXPECT_NEAR(sin(degree{0.0f}), 0.0f, 1e-4f);
  EXPECT_NEAR(cos(degree{0.0f}), 1.0f, 1e-4f);
  EXPECT_NEAR(cos(degree{180.0f}), -1.0f, 1e-4f);
  EXPECT_NEAR(tan(angle{degree{45.0f}}), 1.0f, 1e-3f);
}

TEST(angle_test, angle_addition_and_subtraction) {
  auto sum = angle{degree{30.0f}} + degree{60.0f};
  EXPECT_NEAR(sum.to_degrees().value(), 90.0f, 1e-3f);

  auto difference = angle{degree{90.0f}} - degree{30.0f};
  EXPECT_NEAR(difference.to_degrees().value(), 60.0f, 1e-3f);
}

TEST(angle_test, degree_clamp_respects_bounds) {
  const auto below = clamp(degree{-10.0f}, degree{0.0f}, degree{360.0f});
  const auto above = clamp(degree{500.0f}, degree{0.0f}, degree{360.0f});
  const auto inside = clamp(degree{45.0f}, degree{0.0f}, degree{360.0f});

  EXPECT_FLOAT_EQ(below.value(), 0.0f);
  EXPECT_FLOAT_EQ(above.value(), 360.0f);
  EXPECT_FLOAT_EQ(inside.value(), 45.0f);
}

TEST(angle_test, user_defined_literals_construct_expected_values) {
  EXPECT_FLOAT_EQ((90.0_deg).value(), 90.0f);
  EXPECT_NEAR((1.0_rad).value(), 1.0f, 1e-6f);
}
