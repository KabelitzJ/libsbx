// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <gtest/gtest.h>

#include <libsbx/graphics/types.hpp>

using namespace sbx::graphics;

TEST(sample_count_test, clamps_to_highest_supported_count) {
  const auto up_to_4 = VkSampleCountFlags{VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT};

  EXPECT_EQ(clamp_sample_count(8u, up_to_4), samples::count_4);
  EXPECT_EQ(clamp_sample_count(4u, up_to_4), samples::count_4);
  EXPECT_EQ(clamp_sample_count(2u, up_to_4), samples::count_2);
  EXPECT_EQ(clamp_sample_count(1u, up_to_4), samples::count_1);
  EXPECT_EQ(clamp_sample_count(3u, up_to_4), samples::count_2);
  EXPECT_EQ(clamp_sample_count(0u, up_to_4), samples::count_1);
  EXPECT_EQ(clamp_sample_count(8u, VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_8_BIT), samples::count_8);
  EXPECT_EQ(clamp_sample_count(4u, VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_8_BIT), samples::count_1);
}
