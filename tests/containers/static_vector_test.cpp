// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/containers/static_vector.hpp>

using namespace sbx::containers;

TEST(static_vector_test, default_constructed_vector_is_empty) {
  const auto vector = static_vector<std::int32_t, 4u>{};

  EXPECT_TRUE(vector.is_empty());
  EXPECT_FALSE(vector.is_full());
  EXPECT_EQ(vector.size(), 0u);
  EXPECT_EQ(vector.capacity(), 4u);
}

TEST(static_vector_test, push_back_grows_up_to_capacity) {
  auto vector = static_vector<std::int32_t, 3u>{};

  vector.push_back(1);
  vector.push_back(2);
  vector.push_back(3);

  EXPECT_EQ(vector.size(), 3u);
  EXPECT_TRUE(vector.is_full());
  EXPECT_EQ(vector[0], 1);
  EXPECT_EQ(vector[1], 2);
  EXPECT_EQ(vector[2], 3);
}

TEST(static_vector_test, push_back_beyond_capacity_is_a_no_op) {
  auto vector = static_vector<std::int32_t, 2u>{};

  vector.push_back(1);
  vector.push_back(2);
  vector.push_back(3); // Should be silently dropped; the vector is already full.

  EXPECT_EQ(vector.size(), 2u);
  EXPECT_EQ(vector[1], 2);
}

TEST(static_vector_test, pop_back_removes_the_last_element) {
  auto vector = static_vector<std::int32_t, 4u>{1, 2, 3};

  vector.pop_back();

  EXPECT_EQ(vector.size(), 2u);
  EXPECT_EQ(vector.back(), 2);
}

TEST(static_vector_test, pop_back_on_empty_vector_is_a_no_op) {
  auto vector = static_vector<std::int32_t, 4u>{};

  vector.pop_back();

  EXPECT_TRUE(vector.is_empty());
}

TEST(static_vector_test, front_and_back_return_the_boundary_elements) {
  const auto vector = static_vector<std::int32_t, 4u>{10, 20, 30};

  EXPECT_EQ(vector.front(), 10);
  EXPECT_EQ(vector.back(), 30);
}

TEST(static_vector_test, clear_empties_the_vector) {
  auto vector = static_vector<std::int32_t, 4u>{1, 2, 3};

  vector.clear();

  EXPECT_TRUE(vector.is_empty());
  EXPECT_EQ(vector.size(), 0u);
}

TEST(static_vector_test, iteration_visits_elements_in_order) {
  const auto vector = static_vector<std::int32_t, 4u>{1, 2, 3};

  auto sum = 0;
  for (const auto value : vector) {
    sum += value;
  }

  EXPECT_EQ(sum, 6);
}

TEST(static_vector_test, copy_constructor_duplicates_contents) {
  const auto original = static_vector<std::int32_t, 4u>{1, 2, 3};
  const auto copy = original;

  EXPECT_EQ(copy.size(), original.size());
  EXPECT_TRUE(copy == original);
}

TEST(static_vector_test, move_constructor_transfers_contents) {
  auto original = static_vector<std::int32_t, 4u>{1, 2, 3};
  const auto moved = std::move(original);

  EXPECT_EQ(moved.size(), 3u);
  EXPECT_EQ(moved[0], 1);
}

TEST(static_vector_test, equality_compares_elements_not_capacity) {
  const auto a = static_vector<std::int32_t, 4u>{1, 2, 3};
  const auto b = static_vector<std::int32_t, 4u>{1, 2, 3};
  const auto c = static_vector<std::int32_t, 4u>{1, 2, 4};

  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
}
