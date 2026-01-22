// SPDX-License-Identifier: MIT
#include <libsbx/utility/compression.hpp>

#include <lz4.h>

namespace sbx::utility {

auto basic_compressor<compression_type::lz4>::compress(std::span<const char> input) -> std::vector<char> {
  auto compressed = std::vector<char>{};
  compressed.resize(LZ4_compressBound(static_cast<int>(input.size())));

  const auto compressed_size = LZ4_compress_default(input.data(), compressed.data(), input.size(), compressed.size());

  if (compressed_size <= 0) {
    throw compression_error{"LZ4 compression failed"};
  }

  compressed.resize(compressed_size);

  return compressed;
}

auto basic_compressor<compression_type::lz4>::decompress(std::span<const char> input, std::size_t original_size) -> std::vector<char> {
  auto decompressed = std::vector<char>{};
  decompressed.resize(original_size);
  
  const int result = LZ4_decompress_safe(input.data(), decompressed.data(), static_cast<int>(input.size()), static_cast<int>(original_size));

  if (result < 0) {
    throw decompression_error("LZ4 decompression failed.");
  }

  decompressed.resize(result);

  return decompressed;
}

} // namespace sbx::utility
