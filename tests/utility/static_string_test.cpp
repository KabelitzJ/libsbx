// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <stdexcept>

#include <gtest/gtest.h>

#include <libsbx/utility/static_string.hpp>

using namespace sbx::utility;

TEST(static_string_test, constructed_from_a_view_stores_its_content) {
  const auto value = static_string<16u>{"hello"};

  EXPECT_EQ(value.size(), 5u);
  EXPECT_STREQ(value.c_str(), "hello");
  EXPECT_EQ(value.view(), "hello");
}

TEST(static_string_test, push_back_and_pop_back_grow_and_shrink_the_string) {
  auto value = static_string<8u>{};

  value.push_back('h');
  value.push_back('i');

  EXPECT_EQ(value.view(), "hi");

  value.pop_back();

  EXPECT_EQ(value.view(), "h");
}

TEST(static_string_test, push_back_beyond_capacity_throws) {
  auto value = static_string<2u>{"hi"};

  EXPECT_THROW(value.push_back('!'), std::length_error);
}

TEST(static_string_test, construction_beyond_capacity_throws) {
  EXPECT_THROW((static_string<2u>{"too long"}), std::length_error);
}

TEST(static_string_test, append_concatenates_within_capacity) {
  auto value = static_string<16u>{"hello"};

  value.append(" world");

  EXPECT_EQ(value.view(), "hello world");
}

TEST(static_string_test, clear_empties_the_string) {
  auto value = static_string<16u>{"hello"};

  value.clear();

  EXPECT_TRUE(value.empty());
  EXPECT_EQ(value.size(), 0u);
}

TEST(static_string_test, equality_compares_by_content) {
  const auto a = static_string<16u>{"hello"};

  EXPECT_TRUE(a == static_string<16u>{"hello"});
  EXPECT_FALSE(a == static_string<16u>{"world"});
}

TEST(static_string_test, starts_with_and_ends_with_check_substrings) {
  const auto value = static_string<16u>{"hello world"};

  EXPECT_TRUE(value.starts_with("hello"));
  EXPECT_TRUE(value.ends_with("world"));
  EXPECT_FALSE(value.starts_with("world"));
}

TEST(static_string_test, substr_returns_a_view_into_the_string) {
  const auto value = static_string<16u>{"hello world"};

  EXPECT_EQ(value.substr(6), "world");
  EXPECT_EQ(value.substr(0, 5), "hello");
}
