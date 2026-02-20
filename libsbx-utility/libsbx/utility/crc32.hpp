// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_CRC32_HPP_
#define LIBSBX_UTILITY_CRC32_HPP_

#include <cstdint>

#include <span>
#include <concepts>

namespace sbx::utility {

auto crc32(std::span<const std::uint8_t> buffer, std::uint32_t crc = 0xFFFFFFFFu) -> std::uint32_t;

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