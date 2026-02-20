// SPDX-License-Identifier: MIT
#include <libsbx/utility/compression.hpp>

#include <lz4.h>

#include <zstd.h>

namespace sbx::utility {

auto basic_compressor<compression_type::lz4>::compress(std::span<const char> input) -> std::vector<char> {
  auto compressed = std::vector<char>{};
  compressed.resize(LZ4_compressBound(static_cast<int>(input.size())));

  const auto compressed_size = LZ4_compress_default(input.data(), compressed.data(), static_cast<int>(input.size()), static_cast<int>(compressed.size()));

  if (compressed_size <= 0) {
    throw compression_error{"LZ4 compression failed"};
  }

  compressed.resize(compressed_size);

  return compressed;
}

auto basic_compressor<compression_type::lz4>::decompress(std::span<const char> input, std::span<char> output) -> void {
  const auto result = LZ4_decompress_safe(input.data(), output.data(), static_cast<int>(input.size()), static_cast<int>(output.size()));

  if (result < 0) {
    throw decompression_error{"LZ4 decompression failed"};
  }
}

auto basic_compressor<compression_type::zstd>::compress(std::span<const char> input) -> std::vector<char> {
  auto compressed = std::vector<char>{};
  compressed.resize(ZSTD_compressBound(input.size()));

  const auto compressed_size = ZSTD_compress(compressed.data(), compressed.size(), input.data(), input.size(), ZSTD_defaultCLevel());

  if (ZSTD_isError(compressed_size)) {
    throw compression_error{ZSTD_getErrorName(compressed_size)};
  }

  compressed.resize(compressed_size);

  return compressed;
}

auto basic_compressor<compression_type::zstd>::decompress(std::span<const char> input, std::span<char> output) -> void {
  const auto result = ZSTD_decompress(output.data(), output.size(), input.data(), input.size());

  if (ZSTD_isError(result)) {
    throw decompression_error{ZSTD_getErrorName(result)};
  }
}

} // namespace sbx::utility
