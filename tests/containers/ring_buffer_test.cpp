// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <vector>

#include <gtest/gtest.h>

#include <libsbx/containers/ring_buffer.hpp>

using namespace sbx::containers;

TEST(ring_buffer_test, freshly_constructed_buffer_is_empty) {
  const auto buffer = ring_buffer<std::int32_t>{4u};

  EXPECT_TRUE(buffer.is_empty());
  EXPECT_FALSE(buffer.is_full());
  EXPECT_EQ(buffer.size(), 0u);
  EXPECT_EQ(buffer.capacity(), 4u);
}

TEST(ring_buffer_test, push_below_capacity_grows_in_logical_order) {
  auto buffer = ring_buffer<std::int32_t>{4u};

  buffer.push(1);
  buffer.push(2);
  buffer.push(3);

  EXPECT_EQ(buffer.size(), 3u);
  EXPECT_FALSE(buffer.is_full());
  EXPECT_EQ(buffer[0], 1); // Oldest.
  EXPECT_EQ(buffer[2], 3); // Newest.
}

TEST(ring_buffer_test, push_beyond_capacity_overwrites_the_oldest_element) {
  auto buffer = ring_buffer<std::int32_t>{3u};

  buffer.push(1);
  buffer.push(2);
  buffer.push(3);
  buffer.push(4); // Overwrites 1.

  EXPECT_TRUE(buffer.is_full());
  EXPECT_EQ(buffer.size(), 3u);
  EXPECT_EQ(buffer[0], 2); // Now the oldest.
  EXPECT_EQ(buffer[1], 3);
  EXPECT_EQ(buffer[2], 4); // Newest.
}

TEST(ring_buffer_test, repeated_overwrites_keep_logical_order_correct) {
  auto buffer = ring_buffer<std::int32_t>{3u};

  for (auto value = 1; value <= 10; ++value) {
    buffer.push(value);
  }

  // Last 3 pushed values, oldest to newest.
  EXPECT_EQ(buffer[0], 8);
  EXPECT_EQ(buffer[1], 9);
  EXPECT_EQ(buffer[2], 10);
}

TEST(ring_buffer_test, iteration_visits_elements_oldest_to_newest) {
  auto buffer = ring_buffer<std::int32_t>{3u};

  buffer.push(1);
  buffer.push(2);
  buffer.push(3);
  buffer.push(4);

  auto collected = std::vector<std::int32_t>{};
  for (const auto value : buffer) {
    collected.push_back(value);
  }

  EXPECT_EQ(collected, (std::vector<std::int32_t>{2, 3, 4}));
}

TEST(ring_buffer_test, clear_empties_the_buffer_and_resets_overwrite_position) {
  auto buffer = ring_buffer<std::int32_t>{3u};

  buffer.push(1);
  buffer.push(2);
  buffer.clear();

  EXPECT_TRUE(buffer.is_empty());

  buffer.push(10);
  EXPECT_EQ(buffer[0], 10);
}
