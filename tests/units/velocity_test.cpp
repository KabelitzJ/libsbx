// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/units/velocity.hpp>
#include <libsbx/units/length.hpp>
#include <libsbx/units/time.hpp>
#include <libsbx/units/area.hpp>

using namespace sbx::units;

TEST(velocity_test, dividing_length_by_time_yields_velocity) {
  const auto speed = meters{100.0f} / seconds{10.0f};

  EXPECT_TRUE(speed == meters_per_second{10.0f});
}

TEST(velocity_test, dividing_velocity_by_time_yields_acceleration) {
  const auto acceleration = meters_per_second{10.0f} / seconds{2.0f};

  EXPECT_FLOAT_EQ(acceleration.value(), 5.0f);
}

TEST(velocity_test, multiplying_two_lengths_yields_an_area) {
  const auto area = meters{5.0f} * meters{4.0f};

  EXPECT_TRUE(area == square_meters{20.0f});
}

TEST(velocity_test, dividing_across_mismatched_scales_still_yields_the_correct_physical_speed) {
  const auto speed = meters_per_second{kilometers{1.0f} / hours{1.0f}};

  // 1 km/h = 1000 m / 3600 s.
  EXPECT_NEAR(speed.value(), 1000.0f / 3600.0f, 1e-4f);
}
