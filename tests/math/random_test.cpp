// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

#include <libsbx/math/random.hpp>
#include <libsbx/math/traits.hpp>

using namespace sbx::math;

TEST(limit_traits_test, min_is_the_lowest_value_for_every_numeric_type) {
  EXPECT_EQ(limit_traits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::min());
  EXPECT_EQ(limit_traits<std::uint32_t>::min(), 0u);
  EXPECT_EQ(limit_traits<std::float_t>::min(), -std::numeric_limits<std::float_t>::max());
  EXPECT_EQ(limit_traits<std::double_t>::min(), -std::numeric_limits<std::double_t>::max());
}

TEST(random_test, full_float_range_draws_are_finite_and_signed_both_ways) {
  random::seed(42u);

  auto negative = false;
  auto positive = false;

  for (auto i = 0; i < 1000; ++i) {
    const auto value = random::next<std::float_t>();

    ASSERT_TRUE(std::isfinite(value));

    negative |= value < 0.0f;
    positive |= value > 0.0f;
  }

  EXPECT_TRUE(negative);
  EXPECT_TRUE(positive);
}

TEST(random_test, bounded_draws_stay_in_range) {
  random::seed(7u);

  for (auto i = 0; i < 1000; ++i) {
    const auto value = random::next<std::float_t>(-2.0f, 3.0f);

    ASSERT_GE(value, -2.0f);
    ASSERT_LE(value, 3.0f);
  }
}
