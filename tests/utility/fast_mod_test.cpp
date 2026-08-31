// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include <libsbx/utility/fast_mod.hpp>

using namespace sbx::utility;

TEST(fast_mod_test, unsigned_value_below_modulus_is_returned_unchanged) {
  EXPECT_EQ(fast_mod(std::uint32_t{3u}, std::uint32_t{8u}), 3u);
}

TEST(fast_mod_test, unsigned_value_matches_the_builtin_modulus_operator) {
  for (auto value = std::uint32_t{0u}; value < 50u; ++value) {
    EXPECT_EQ(fast_mod(value, std::uint32_t{7u}), value % 7u);
  }
}

TEST(fast_mod_test, unsigned_value_equal_to_modulus_wraps_to_zero) {
  EXPECT_EQ(fast_mod(std::uint32_t{8u}, std::uint32_t{8u}), 0u);
}

TEST(fast_mod_test, compile_time_modulus_overload_matches_the_runtime_one) {
  for (auto value = std::uint32_t{0u}; value < 50u; ++value) {
    EXPECT_EQ((fast_mod<std::uint32_t, std::size_t{7u}>(value)), value % 7u);
  }
}

TEST(fast_mod_test, floating_point_value_matches_fmod) {
  EXPECT_FLOAT_EQ(fast_mod(10.5f, 3.0f), std::fmod(10.5f, 3.0f));
  EXPECT_FLOAT_EQ(fast_mod(1.5f, 3.0f), 1.5f); // Below the modulus: returned unchanged.
}
