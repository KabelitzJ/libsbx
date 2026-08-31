// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <string>

#include <gtest/gtest.h>

#include <libsbx/containers/dense_map.hpp>

using namespace sbx::containers;

TEST(dense_map_test, freshly_constructed_map_is_empty) {
  const auto map = dense_map<std::int32_t, std::string>{};

  EXPECT_TRUE(map.empty());
  EXPECT_EQ(map.size(), 0u);
}

TEST(dense_map_test, insert_adds_a_new_entry_and_reports_success) {
  auto map = dense_map<std::int32_t, std::string>{};

  const auto [iterator, inserted] = map.insert({1, "one"});

  EXPECT_TRUE(inserted);
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(iterator->second, "one");
}

TEST(dense_map_test, inserting_an_existing_key_does_not_overwrite_and_reports_failure) {
  auto map = dense_map<std::int32_t, std::string>{};

  map.insert({1, "one"});
  const auto [iterator, inserted] = map.insert({1, "uno"});

  EXPECT_FALSE(inserted);
  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(iterator->second, "one");
}

TEST(dense_map_test, operator_bracket_inserts_a_default_value_for_a_missing_key) {
  auto map = dense_map<std::int32_t, std::string>{};

  map[1] = "one";

  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map.at(1), "one");
}

TEST(dense_map_test, find_and_contains_reflect_membership) {
  auto map = dense_map<std::int32_t, std::string>{};
  map.insert({1, "one"});

  EXPECT_TRUE(map.contains(1));
  EXPECT_FALSE(map.contains(2));
  EXPECT_NE(map.find(1), map.end());
  EXPECT_EQ(map.find(2), map.end());
}

TEST(dense_map_test, erase_by_key_removes_the_entry) {
  auto map = dense_map<std::int32_t, std::string>{};
  map.insert({1, "one"});
  map.insert({2, "two"});

  const auto erased = map.erase(1);

  EXPECT_TRUE(erased);
  EXPECT_EQ(map.size(), 1u);
  EXPECT_FALSE(map.contains(1));
  EXPECT_TRUE(map.contains(2));
}

TEST(dense_map_test, erase_of_a_missing_key_reports_failure) {
  auto map = dense_map<std::int32_t, std::string>{};

  EXPECT_FALSE(map.erase(1));
}

TEST(dense_map_test, iteration_visits_every_inserted_pair) {
  auto map = dense_map<std::int32_t, std::string>{};
  map.insert({1, "one"});
  map.insert({2, "two"});
  map.insert({3, "three"});

  auto visited = std::size_t{0};
  for (const auto& [key, value] : map) {
    EXPECT_FALSE(value.empty());
    ++visited;
  }

  EXPECT_EQ(visited, 3u);
}

TEST(dense_map_test, survives_growth_past_the_initial_bucket_count) {
  auto map = dense_map<std::int32_t, std::int32_t>{};

  for (auto key = 0; key < 256; ++key) {
    map.insert({key, key * 2});
  }

  EXPECT_EQ(map.size(), 256u);

  for (auto key = 0; key < 256; ++key) {
    ASSERT_TRUE(map.contains(key));
    EXPECT_EQ(map.at(key), key * 2);
  }
}
