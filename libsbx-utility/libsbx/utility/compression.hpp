// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_COMPRESSION_HPP_
#define LIBSBX_UTILITY_COMPRESSION_HPP_

#include <span>
#include <vector>
#include <stdexcept>
#include <cstdint>

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
  lz4 = 0
}; // enum class compression_type

template<compression_type Type>
struct basic_compressor {
  [[nodiscard]] static auto compress(std::span<const char> input) -> std::vector<char>;
  [[nodiscard]] static auto decompress(std::span<const char> input, const std::size_t original_size) -> std::vector<char>;
}; // struct basic_compressor

template<>
struct basic_compressor<compression_type::lz4> {
  [[nodiscard]] static auto compress(std::span<const char> input) -> std::vector<char>;
  [[nodiscard]] static auto decompress(std::span<const char> input, const std::size_t original_size) -> std::vector<char>;
}; // struct basic_compressor

using compressor = basic_compressor<compression_type::lz4>;

template<typename Type, compression_type CompressionType = compression_type::lz4>
auto compress(std::span<const Type> input) -> std::vector<char> {
  return basic_compressor<CompressionType>::compress({reinterpret_cast<const char*>(input.data()), input.size() * sizeof(Type)});
}

template<typename Type, compression_type CompressionType = compression_type::lz4>
auto decompress(std::span<const char> input, const std::size_t original_size) -> std::vector<Type> {
  return basic_compressor<CompressionType>::decompress({reinterpret_cast<const char*>(input.data()), input.size() * sizeof(Type)}, original_size * sizeof(Type));
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_COMPRESSION_HPP_
