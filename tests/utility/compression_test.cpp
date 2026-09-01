// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <libsbx/utility/compression.hpp>

using namespace sbx::utility;

const auto sample_text = std::string{
  "The quick brown fox jumps over the lazy dog. "
  "The quick brown fox jumps over the lazy dog. "
  "The quick brown fox jumps over the lazy dog."
};

TEST(compression_test, lz4_round_trip_recovers_the_original_data) {
  const auto compressed = lz4_compressor::compress({sample_text.data(), sample_text.size()});

  auto decompressed = std::vector<char>(sample_text.size());
  lz4_compressor::decompress({compressed.data(), compressed.size()}, {decompressed.data(), decompressed.size()});

  EXPECT_EQ(std::string(decompressed.data(), decompressed.size()), sample_text);
}

TEST(compression_test, zstd_round_trip_recovers_the_original_data) {
  const auto compressed = zstd_compressor::compress({sample_text.data(), sample_text.size()});

  auto decompressed = std::vector<char>(sample_text.size());
  zstd_compressor::decompress({compressed.data(), compressed.size()}, {decompressed.data(), decompressed.size()});

  EXPECT_EQ(std::string(decompressed.data(), decompressed.size()), sample_text);
}

TEST(compression_test, repetitive_input_compresses_smaller_than_the_original) {
  const auto compressed = lz4_compressor::compress({sample_text.data(), sample_text.size()});

  EXPECT_LT(compressed.size(), sample_text.size());
}

TEST(compression_test, typed_compress_and_decompress_round_trip_a_vector_of_ints) {
  const auto original = std::vector<std::int32_t>{1, 2, 3, 4, 5, 6, 7, 8};

  const auto compressed = compress<std::int32_t>(std::span{original});
  const auto decompressed = decompress<std::int32_t>(std::span{compressed}, original.size());

  EXPECT_EQ(decompressed, original);
}
