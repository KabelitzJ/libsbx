// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/ecs/entity.hpp>

using namespace sbx::ecs;

namespace {

using traits = entity_traits<sbx::ecs::entity>;

} // namespace

TEST(entity_test, construct_and_decompose_round_trips_id_and_version) {
  const auto value = traits::construct(traits::entity_type{42u}, traits::version_type{7u});

  EXPECT_EQ(traits::to_entity(value), traits::entity_type{42u});
  EXPECT_EQ(traits::to_version(value), traits::version_type{7u});
}

TEST(entity_test, construct_defaults_to_version_zero) {
  const auto value = traits::construct(traits::entity_type{5u});

  EXPECT_EQ(traits::to_entity(value), traits::entity_type{5u});
  EXPECT_EQ(traits::to_version(value), traits::version_type{0u});
}

TEST(entity_test, next_bumps_the_version_but_keeps_the_id) {
  const auto original = traits::construct(traits::entity_type{5u}, traits::version_type{0u});
  const auto bumped = traits::next(original);

  EXPECT_EQ(traits::to_entity(bumped), traits::to_entity(original));
  EXPECT_EQ(traits::to_version(bumped), traits::version_type{1u});
}

TEST(entity_test, different_ids_or_versions_produce_different_encoded_values) {
  const auto a = traits::construct(traits::entity_type{1u}, traits::version_type{0u});
  const auto b = traits::construct(traits::entity_type{2u}, traits::version_type{0u});
  const auto c = traits::construct(traits::entity_type{1u}, traits::version_type{1u});

  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
}

TEST(entity_test, null_entity_compares_equal_by_id_regardless_of_version) {
  const auto with_id_matching_null = traits::construct(traits::entity_mask, traits::version_type{3u});

  EXPECT_TRUE(null_entity == with_id_matching_null);

  const auto other = traits::construct(traits::entity_type{1u}, traits::version_type{0u});
  EXPECT_FALSE(null_entity == other);
}

TEST(entity_test, tombstone_entity_compares_equal_by_version_regardless_of_id) {
  const auto with_version_matching_tombstone = traits::construct(traits::entity_type{9u}, traits::version_mask);

  EXPECT_TRUE(tombstone_entity == with_version_matching_tombstone);

  const auto other = traits::construct(traits::entity_type{9u}, traits::version_type{0u});
  EXPECT_FALSE(tombstone_entity == other);
}
