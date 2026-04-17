// SPDX-License-Identifier: MIT
/* 
 * Copyright (c) 2022 Jonas Kabelitz
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * You should have received a copy of the MIT License along with this program.
 * If not, see <https://opensource.org/licenses/MIT/>.
 */

/**
 * @file libsbx/utility/hash.hpp
 */

#ifndef LIBSBX_UTILITY_HASH_HPP_
#define LIBSBX_UTILITY_HASH_HPP_

/**
 * @ingroup libsbx-utility
 */

#include <utility>
#include <string>
#include <string_view>
#include <span>
#include <iostream>
#include <concepts>
#include <cinttypes>

namespace sbx::utility {

/**
 * @brief A concept that represents a type that can be hashed.
 */
template<typename Type>
concept hashable = requires(const Type& instance) {
  { std::hash<Type>{}(instance) } -> std::same_as<std::size_t>;
}; // concept hashable

/**
 * @brief Combines multiple hashes into a single hash.
 */
inline constexpr auto hash_combine([[maybe_unused]] std::size_t& seed) -> void { }

/**
 * @brief Combines multiple hashes into a single hash.
 * 
 * @tparam Type The type of the first value to hash.
 * @tparam Rest The types of the remaining values to hash.
 * 
 * @param seed The seed to combine the hashes with.
 * @param value The first value to hash.
 * @param rest The remaining values to hash.
 */
template<hashable Type, hashable... Rest>
inline constexpr auto hash_combine(std::size_t& seed, const Type& value, Rest... rest) -> void {
  auto hasher = std::hash<Type>{};
  seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  (hash_combine(seed, rest), ...);
}

/**
 * @brief A concept that represents a character type.
 * 
 * @tparam Type The type to check.
 */
template<typename Type>
concept character = std::same_as<Type, char> || std::same_as<Type, wchar_t> || std::same_as<Type, char8_t> || std::same_as<Type, char16_t> || std::same_as<Type, char32_t>;

/**
 * @brief Traits for the fnv1a hash function.
 * 
 * @tparam Type The type of the hash.
 */
template<std::unsigned_integral Type>
struct fnv1a_traits;

/**
 * @brief Traits for the fnv1a hash function specialized for 32-bit hashes.
 * 
 * @see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV_hash_parameters
 */
template<>
struct fnv1a_traits<std::uint32_t> {
  /** @brief The offset basis for 32-bit fnv1a. */
  inline static constexpr auto basis = std::uint32_t{0x811c9dc5};
  /** @brief The prime for 32-bit fnv1a. */
  inline static constexpr auto prime = std::uint32_t{0x01000193};
}; // struct fnv1a_traits

/**
 * @brief Traits for the fnv1a hash function specialized for 64-bit hashes.
 * 
 * @see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function#FNV_hash_parameters
 */
template<>
struct fnv1a_traits<std::uint64_t> {
  /** @brief The offset basis for 64-bit fnv1a. */
  inline static constexpr auto basis = std::uint64_t{0xcbf29ce484222325};
  /** @brief The prime for 64-bit fnv1a. */
  inline static constexpr auto prime = std::uint64_t{0x00000100000001B3};
}; // struct fnv1a_traits

/**
 * @brief Functor that implements the fnv1a hash algorithm.
 * 
 * @tparam Char The character type of the string to hash.
 * @tparam Hash The type of the hash. Defaults to std::uint64_t.
 * @tparam HashTraits The traits of the hash. Defaults to fnv1a_traits<Hash>.
 */
template<character Char, std::unsigned_integral Hash = std::uint64_t, typename HashTraits = fnv1a_traits<Hash>>
struct fnv1a_hash {
  using char_type = Char;
  using size_type = std::size_t;
  using hash_type = Hash;
  using hash_traits = HashTraits;

  /**
   * @brief Hashes the given string.
   * 
   * @param string The string to hash. 
   * @param size The size of the string.
   * 
   * @return hash_type The hash of the string.
   */
  inline constexpr auto operator()(std::basic_string_view<Char> string) const noexcept -> hash_type {
    auto hash = hash_traits::basis;

    for (const auto& character : string) {
      hash ^= static_cast<hash_type>(character);
      hash *= hash_traits::prime;
    }

    return hash;
  }
}; // struct fnv1a_hash

template<std::unsigned_integral Hash = std::uint64_t>
struct djb2_hash {

  using hash_type = Hash;

  inline constexpr auto operator()(std::span<const std::uint8_t> buffer) const noexcept -> hash_type {
    // Implementation from https://theartincode.stanis.me/008-djb2/
    auto hash = hash_type{5381};

    for (auto byte : buffer) {
      hash = ((hash << 5) + hash) + static_cast<std::int32_t>(byte);
    }

    return hash;
  }

  template<typename Type>
  requires (std::is_trivially_copyable_v<Type>)
  inline constexpr auto operator()(const Type& value) const noexcept -> hash_type {
    return operator()({reinterpret_cast<std::uint8_t*>(std::addressof(value)), sizeof(Type)});
  }

}; // struct djb2_hash

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_HASH_HPP_
