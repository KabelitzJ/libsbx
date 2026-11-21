// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <libsbx/utility/base64.hpp>

namespace utility = sbx::utility;

auto encode(std::string_view text) -> std::string {
  return utility::base64_encode(std::span<const char>{text.data(), text.size()});
}

TEST(base64, encodes_the_rfc_4648_test_vectors) {
  EXPECT_EQ(encode(""), "");
  EXPECT_EQ(encode("f"), "Zg==");
  EXPECT_EQ(encode("fo"), "Zm8=");
  EXPECT_EQ(encode("foo"), "Zm9v");
  EXPECT_EQ(encode("foob"), "Zm9vYg==");
  EXPECT_EQ(encode("fooba"), "Zm9vYmE=");
  EXPECT_EQ(encode("foobar"), "Zm9vYmFy");
}

TEST(base64, round_trips_every_byte_value) {
  auto bytes = std::vector<char>{};

  for (auto value = 0; value < 256; ++value) {
    bytes.push_back(static_cast<char>(value));
  }

  for (auto length = std::size_t{0u}; length <= bytes.size(); length += 37u) {
    const auto input = std::span<const char>{bytes.data(), length};
    const auto decoded = utility::base64_decode(utility::base64_encode(input));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, (std::vector<char>{input.begin(), input.end()}));
  }
}

TEST(base64, rejects_invalid_text) {
  EXPECT_FALSE(utility::base64_decode("Zg=").has_value());
  EXPECT_FALSE(utility::base64_decode("Z!==").has_value());
}
