// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/units/time.hpp>

using namespace sbx::units;
using namespace sbx::units::literals;

TEST(time_test, converting_between_scales_preserves_the_physical_duration) {
  EXPECT_FLOAT_EQ(seconds{minutes{1.0f}}.value(), 60.0f);
  EXPECT_FLOAT_EQ(seconds{hours{1.0f}}.value(), 3600.0f);
  EXPECT_FLOAT_EQ(milliseconds{seconds{1.0f}}.value(), 1000.0f);
}

TEST(time_test, addition_across_scales_converts_to_the_left_hand_scale) {
  const auto sum = minutes{1.0f} + seconds{30.0f};

  EXPECT_FLOAT_EQ(sum.value(), 1.5f);
}

TEST(time_test, comparison_works_across_scales) {
  EXPECT_TRUE(hours{1.0f} == minutes{60.0f});
  EXPECT_TRUE(minutes{2.0f} > seconds{100.0f});
  EXPECT_TRUE(milliseconds{500.0f} < seconds{1.0f});
}

TEST(time_test, user_defined_literals_construct_the_expected_scale) {
  EXPECT_TRUE(1.0_h == minutes{60.0f});
  EXPECT_TRUE(1.0_min == seconds{60.0f});
  EXPECT_TRUE(500.0_ms == seconds{0.5f});
}
