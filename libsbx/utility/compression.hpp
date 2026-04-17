// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_COMPRESSION_HPP_
#define LIBSBX_UTILITY_COMPRESSION_HPP_

#include <span>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <cstring>

namespace sbx::utility {

struct compression_error : public std::runtime_error {
  explicit compression_error(std::string_view message)
  : std::runtime_error{std::string{message}} { }
}; // struct compression_error

struct decompression_error : public std::runtime_error {
  explicit decompression_error(std::string_view message)
  : std::runtime_error{std::string{message}} { }
}; // struct decompression_error

enum class compression_type : std::uint8_t {
  lz4 = 0,
  zstd = 1
}; // enum class compression_type

template<compression_type Type>
struct basic_compressor {
  [[nodiscard]] static auto compress(std::span<const char> input) -> std::vector<char>;
  static auto decompress(std::span<const char> input, std::span<char> output) -> void;
}; // struct basic_compressor

template<>
struct basic_compressor<compression_type::lz4> {
  [[nodiscard]] static auto compress(std::span<const char> input) -> std::vector<char>;
  static auto decompress(std::span<const char> input, std::span<char> output) -> void;
}; // struct basic_compressor

template<>
struct basic_compressor<compression_type::zstd> {
  [[nodiscard]] static auto compress(std::span<const char> input) -> std::vector<char>;
  static auto decompress(std::span<const char> input, std::span<char> output) -> void;
}; // struct basic_compressor

using lz4_compressor  = basic_compressor<compression_type::lz4>;
using zstd_compressor = basic_compressor<compression_type::zstd>;

template<typename Type, compression_type CompressionType = compression_type::lz4>
auto compress(std::span<const Type> input) -> std::vector<char> {
  return basic_compressor<CompressionType>::compress({reinterpret_cast<const char*>(input.data()), input.size() * sizeof(Type)});
}

template<typename Type, compression_type CompressionType = compression_type::lz4>
auto decompress(std::span<const char> input, const std::size_t original_size) -> std::vector<Type> {
  auto output = std::vector<Type>(original_size);

  basic_compressor<CompressionType>::decompress(input, {reinterpret_cast<char*>(output.data()), original_size * sizeof(Type)});

  return output;
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_COMPRESSION_HPP_
