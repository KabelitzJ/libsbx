// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <tuple>
#include <utility>
#include <cmath>

#include <gtest/gtest.h>

#include <libsbx/containers/compressed_pair.hpp>

using namespace sbx::containers;

namespace {

// An empty, non-final type: eligible for the empty-base-class optimization.
struct empty_type { };

} // namespace

TEST(compressed_pair_test, first_and_second_return_the_stored_values) {
  auto pair = compressed_pair<std::int32_t, std::double_t>{1, 2.5};

  EXPECT_EQ(pair.first(), 1);
  EXPECT_DOUBLE_EQ(pair.second(), 2.5);
}

TEST(compressed_pair_test, access_values_through_get) {
  auto pair = compressed_pair<std::int32_t, std::double_t>{1, 2.5};

  EXPECT_EQ(pair.get<0>(), 1);
  EXPECT_DOUBLE_EQ(pair.get<1>(), 2.5);
}

TEST(compressed_pair_test, first_and_second_are_mutable) {
  auto pair = compressed_pair<std::int32_t, std::int32_t>{1, 2};

  pair.first() = 10;
  pair.second() = 20;

  EXPECT_EQ(pair.first(), 10);
  EXPECT_EQ(pair.second(), 20);
}

TEST(compressed_pair_test, empty_type_is_compressed_away) {
  // With the empty-base-class optimization applied, an empty second member
  // should not add its own byte to the pair's size.
  EXPECT_EQ(sizeof(compressed_pair<std::int32_t, empty_type>), sizeof(std::int32_t));
}

TEST(compressed_pair_test, non_empty_types_are_not_compressed) {
  EXPECT_GE(sizeof(compressed_pair<std::int32_t, std::double_t>), sizeof(std::int32_t) + sizeof(std::double_t));
}

TEST(compressed_pair_test, piecewise_construct_forwards_arguments_to_each_member) {
  auto pair = compressed_pair<std::pair<std::int32_t, std::int32_t>, std::int32_t>{
    std::piecewise_construct,
    std::forward_as_tuple(1, 2),
    std::forward_as_tuple(3)
  };

  EXPECT_EQ(pair.first().first, 1);
  EXPECT_EQ(pair.first().second, 2);
  EXPECT_EQ(pair.second(), 3);
}

TEST(compressed_pair_test, swap_exchanges_both_members) {
  auto a = compressed_pair<std::int32_t, std::int32_t>{1, 2};
  auto b = compressed_pair<std::int32_t, std::int32_t>{3, 4};

  a.swap(b);

  EXPECT_EQ(a.first(), 3);
  EXPECT_EQ(a.second(), 4);
  EXPECT_EQ(b.first(), 1);
  EXPECT_EQ(b.second(), 2);
}
