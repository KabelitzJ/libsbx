// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/containers/stable_vector.hpp>

using namespace sbx::containers;

TEST(stable_vector_test, freshly_constructed_vector_has_no_elements) {
  const auto vector = stable_vector<std::int32_t, 4u>{};

  EXPECT_EQ(vector.get_element_count(), 0u);
}

TEST(stable_vector_test, insert_returns_a_stable_index_and_reference) {
  auto vector = stable_vector<std::int32_t, 4u>{};

  const auto [index, reference] = vector.insert(42);

  EXPECT_EQ(index, 0u);
  EXPECT_EQ(reference, 42);
  EXPECT_EQ(vector[0], 42);
  EXPECT_EQ(vector.get_element_count(), 1u);
}

TEST(stable_vector_test, indices_are_assigned_sequentially) {
  auto vector = stable_vector<std::int32_t, 4u>{};

  const auto [first_index, first] = vector.insert(1);
  const auto [second_index, second] = vector.insert(2);

  EXPECT_EQ(first_index, 0u);
  EXPECT_EQ(second_index, 1u);
  EXPECT_EQ(vector[0], 1);
  EXPECT_EQ(vector[1], 2);
}

TEST(stable_vector_test, growth_past_a_page_boundary_preserves_earlier_elements) {
  // PageSize of 4 forces a second page allocation once more than 4 elements exist.
  using vector_type = stable_vector<std::int32_t, 4u>;

  auto vector = vector_type{};

  for (auto value = 0; value < 10; ++value) {
    vector.insert(static_cast<std::int32_t>(value));
  }

  EXPECT_EQ(vector.get_element_count(), 10u);

  for (auto value = 0; value < 10; ++value) {
    EXPECT_EQ(vector[static_cast<vector_type::size_type>(value)], value);
  }
}

TEST(stable_vector_test, emplace_back_reserves_a_default_constructed_slot) {
  auto vector = stable_vector<std::int32_t, 4u>{};

  auto [index, reference] = vector.emplace_back();
  reference = 7;

  EXPECT_EQ(index, 0u);
  EXPECT_EQ(vector[0], 7);
}

TEST(stable_vector_test, for_each_visits_every_element) {
  auto vector = stable_vector<std::int32_t, 4u>{};

  vector.insert(1);
  vector.insert(2);
  vector.insert(3);

  auto sum = 0;
  vector.for_each([&sum](std::int32_t value) {
    sum += value;
  });

  EXPECT_EQ(sum, 6);
}

TEST(stable_vector_test, clear_resets_the_element_count) {
  auto vector = stable_vector<std::int32_t, 4u>{};

  vector.insert(1);
  vector.insert(2);
  vector.clear();

  EXPECT_EQ(vector.get_element_count(), 0u);
}

TEST(stable_vector_test, copy_constructor_duplicates_all_elements) {
  auto original = stable_vector<std::int32_t, 4u>{};
  original.insert(1);
  original.insert(2);
  original.insert(3);

  const auto copy = original;

  EXPECT_EQ(copy.get_element_count(), 3u);
  EXPECT_EQ(copy[0], 1);
  EXPECT_EQ(copy[2], 3);
}
