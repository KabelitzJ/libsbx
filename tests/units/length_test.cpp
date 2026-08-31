// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/units/length.hpp>

using namespace sbx::units;
using namespace sbx::units::literals;

TEST(length_test, converting_between_scales_preserves_the_physical_length) {
  const auto one_km = kilometers{1.0f};
  const auto as_meters = meters{one_km};

  EXPECT_FLOAT_EQ(as_meters.value(), 1000.0f);
}

TEST(length_test, meters_to_centimeters_and_millimeters) {
  const auto value = meters{1.0f};

  EXPECT_FLOAT_EQ(centimeters{value}.value(), 100.0f);
  EXPECT_FLOAT_EQ(millimeters{value}.value(), 1000.0f);
}

TEST(length_test, addition_across_scales_converts_to_the_left_hand_scale) {
  const auto sum = meters{1.0f} + centimeters{50.0f};

  EXPECT_FLOAT_EQ(sum.value(), 1.5f);
}

TEST(length_test, subtraction_across_scales_converts_to_the_left_hand_scale) {
  const auto difference = meters{2.0f} - centimeters{50.0f};

  EXPECT_FLOAT_EQ(difference.value(), 1.5f);
}

TEST(length_test, comparison_works_across_scales) {
  EXPECT_TRUE(meters{1.0f} == centimeters{100.0f});
  EXPECT_TRUE(kilometers{1.0f} > meters{999.0f});
  EXPECT_TRUE(millimeters{500.0f} < meters{1.0f});
}

TEST(length_test, scalar_multiplication_and_division) {
  const auto doubled = meters{2.0f} * 2.0f;
  EXPECT_FLOAT_EQ(doubled.value(), 4.0f);

  const auto halved = meters{4.0f} / 2.0f;
  EXPECT_FLOAT_EQ(halved.value(), 2.0f);
}

TEST(length_test, user_defined_literals_construct_the_expected_scale) {
  EXPECT_TRUE(1.0_km == meters{1000.0f});
  EXPECT_TRUE(1.0_cm == meters{0.01f});
  EXPECT_TRUE(250.0_m == kilometers{0.25f});
}
