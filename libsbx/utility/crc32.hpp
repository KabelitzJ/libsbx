// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_CRC32_HPP_
#define LIBSBX_UTILITY_CRC32_HPP_

#include <cstdint>

#include <span>
#include <concepts>

namespace sbx::utility {

/**
 * @brief Computes the CRC-32 (IEEE 802.3, zlib-compatible) checksum of a buffer.
 *
 * @param buffer The bytes to checksum.
 * @param crc The running checksum to continue from; pass a prior result to checksum
 * a buffer incrementally across multiple calls.
 *
 * @return The CRC-32 checksum of buffer.
 */
auto crc32(std::span<const std::uint8_t> buffer, std::uint32_t crc = 0xFFFFFFFFu) -> std::uint32_t;

/**
 * @brief Packs a compile-time string literal into an unsigned integer, most significant
 * byte last (e.g. "PNG\0" as a 32-bit magic number).
 *
 * @tparam Type The unsigned integer type to pack into; its width must match the literal.
 * @tparam N The string literal's array size (its length plus the null terminator).
 *
 * @param string The string literal to pack.
 *
 * @return The packed magic number.
 */
template<std::unsigned_integral Type, std::size_t N>
requires (sizeof(Type) == N - 1u)
static consteval auto make_magic(const char (&string)[N]) -> Type {
  auto result = Type{};

  for (auto i = 0u; i < N - 1u; ++i) {
    result |= static_cast<Type>(static_cast<unsigned char>(string[i])) << (i * 8u);
  }

  return result;
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_CRC32_HPP_
