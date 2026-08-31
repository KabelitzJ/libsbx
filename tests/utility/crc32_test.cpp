// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <gtest/gtest.h>

#include <libsbx/utility/crc32.hpp>

using namespace sbx::utility;

namespace {

auto crc32_of(std::string_view text) -> std::uint32_t {
  return crc32(std::span{reinterpret_cast<const std::uint8_t*>(text.data()), text.size()});
}

} // namespace

TEST(crc32_test, empty_input_matches_the_known_zero_length_checksum) {
  EXPECT_EQ(crc32_of(""), 0x00000000u);
}

TEST(crc32_test, matches_the_standard_check_value_for_the_ascii_digits) {
  // "123456789" is the canonical CRC-32 (IEEE 802.3) check-value test vector.
  EXPECT_EQ(crc32_of("123456789"), 0xCBF43926u);
}

TEST(crc32_test, different_inputs_produce_different_checksums) {
  EXPECT_NE(crc32_of("hello"), crc32_of("world"));
}

TEST(crc32_test, is_deterministic) {
  EXPECT_EQ(crc32_of("libsbx"), crc32_of("libsbx"));
}

TEST(crc32_test, make_magic_packs_characters_little_endian) {
  constexpr auto magic = make_magic<std::uint32_t>("SBXY");

  EXPECT_EQ(magic & 0xFFu, static_cast<std::uint32_t>('S'));
  EXPECT_EQ((magic >> 8) & 0xFFu, static_cast<std::uint32_t>('B'));
  EXPECT_EQ((magic >> 16) & 0xFFu, static_cast<std::uint32_t>('X'));
  EXPECT_EQ((magic >> 24) & 0xFFu, static_cast<std::uint32_t>('Y'));
}
