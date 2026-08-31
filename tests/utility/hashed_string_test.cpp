// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/utility/hashed_string.hpp>

using namespace sbx::utility;
using namespace sbx::utility::literals;

TEST(hashed_string_test, equal_strings_hash_equal) {
  const auto a = hashed_string{"health"};
  const auto b = hashed_string{"health"};

  EXPECT_EQ(a, b);
  EXPECT_EQ(a.hash(), b.hash());
}

TEST(hashed_string_test, different_strings_hash_differently) {
  const auto a = hashed_string{"health"};
  const auto b = hashed_string{"mana"};

  EXPECT_NE(a.hash(), b.hash());
}

TEST(hashed_string_test, preserves_the_original_text) {
  const auto value = hashed_string{"health"};

  EXPECT_STREQ(value.c_str(), "health");
  EXPECT_EQ(value.size(), 6u);
  EXPECT_FALSE(value.is_empty());
}

TEST(hashed_string_test, default_constructed_string_is_empty) {
  const auto value = hashed_string{};

  EXPECT_TRUE(value.is_empty());
  EXPECT_EQ(value.size(), 0u);
}

TEST(hashed_string_test, literal_operator_produces_the_same_hash_as_the_constructor) {
  const auto from_literal = "health"_hs;
  const auto from_constructor = hashed_string{"health"};

  EXPECT_EQ(from_literal, from_constructor);
}

TEST(hashed_string_test, implicitly_converts_to_its_hash_value) {
  const auto value = hashed_string{"health"};

  const auto hash_value = static_cast<hashed_string::hash_type>(value);

  EXPECT_EQ(hash_value, value.hash());
}
