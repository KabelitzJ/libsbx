// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_COMPRESSION_HPP_
#define LIBSBX_UTILITY_COMPRESSION_HPP_

#include <span>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <cstring>

namespace sbx::utility {

/**
 * @brief Thrown when a basic_compressor::compress call fails.
 *
 * @param message Description of the failure.
 */
struct compression_error : public std::runtime_error {
  explicit compression_error(std::string_view message)
  : std::runtime_error{std::string{message}} { }
}; // struct compression_error

/**
 * @brief Thrown when a basic_compressor::decompress call fails.
 *
 * @param message Description of the failure.
 */
struct decompression_error : public std::runtime_error {
  explicit decompression_error(std::string_view message)
  : std::runtime_error{std::string{message}} { }
}; // struct decompression_error

/** @brief The compression backends basic_compressor can be specialized for. */
enum class compression_type : std::uint8_t {
  lz4 = 0,
  zstd = 1
}; // enum class compression_type

/**
 * @brief Byte-buffer compressor for the given backend. Specialized per compression_type;
 * the unspecialized primary template has no implementation.
 *
 * @tparam Type The compression backend.
 */
template<compression_type Type>
struct basic_compressor {
  [[nodiscard]] static auto compress(std::span<const char> input) -> std::vector<char>;
  static auto decompress(std::span<const char> input, std::span<char> output) -> void;
}; // struct basic_compressor

template<>
struct basic_compressor<compression_type::lz4> {

  /**
   * @brief Compresses a buffer with LZ4.
   *
   * @param input The bytes to compress.
   *
   * @return The compressed bytes.
   *
   * @throws compression_error If LZ4 reports a compression failure.
   */
  [[nodiscard]] static auto compress(std::span<const char> input) -> std::vector<char>;

  /**
   * @brief Decompresses an LZ4-compressed buffer into output.
   *
   * @param input The compressed bytes.
   * @param output The buffer to decompress into; must be exactly the original, uncompressed size.
   *
   * @throws decompression_error If LZ4 reports a decompression failure.
   */
  static auto decompress(std::span<const char> input, std::span<char> output) -> void;
}; // struct basic_compressor

template<>
struct basic_compressor<compression_type::zstd> {

  /**
   * @brief Compresses a buffer with Zstandard, at the library's default compression level.
   *
   * @param input The bytes to compress.
   *
   * @return The compressed bytes.
   *
   * @throws compression_error If Zstandard reports a compression failure.
   */
  [[nodiscard]] static auto compress(std::span<const char> input) -> std::vector<char>;

  /**
   * @brief Decompresses a Zstandard-compressed buffer into output.
   *
   * @param input The compressed bytes.
   * @param output The buffer to decompress into; must be exactly the original, uncompressed size.
   *
   * @throws decompression_error If Zstandard reports a decompression failure.
   */
  static auto decompress(std::span<const char> input, std::span<char> output) -> void;
}; // struct basic_compressor

using lz4_compressor  = basic_compressor<compression_type::lz4>;
using zstd_compressor = basic_compressor<compression_type::zstd>;

/**
 * @brief Compresses a typed span as raw bytes.
 *
 * @tparam Type The element type of the input span.
 * @tparam CompressionType The compression backend to use.
 *
 * @param input The elements to compress.
 *
 * @return The compressed bytes.
 *
 * @throws compression_error If the backend reports a compression failure.
 */
template<typename Type, compression_type CompressionType = compression_type::lz4>
auto compress(std::span<const Type> input) -> std::vector<char> {
  return basic_compressor<CompressionType>::compress({reinterpret_cast<const char*>(input.data()), input.size() * sizeof(Type)});
}

/**
 * @brief Decompresses raw bytes back into a typed vector.
 *
 * @tparam Type The element type to reconstruct.
 * @tparam CompressionType The compression backend to use.
 *
 * @param input The compressed bytes.
 * @param original_size The element count of the original, uncompressed data.
 *
 * @return The decompressed elements.
 *
 * @throws decompression_error If the backend reports a decompression failure.
 */
template<typename Type, compression_type CompressionType = compression_type::lz4>
auto decompress(std::span<const char> input, const std::size_t original_size) -> std::vector<Type> {
  auto output = std::vector<Type>(original_size);

  basic_compressor<CompressionType>::decompress(input, {reinterpret_cast<char*>(output.data()), original_size * sizeof(Type)});

  return output;
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_COMPRESSION_HPP_
