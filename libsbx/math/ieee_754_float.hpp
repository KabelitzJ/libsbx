// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MATH_IEEE_754_FLOAT_HPP_
#define LIBSBX_MATH_IEEE_754_FLOAT_HPP_

#include <cmath>
#include <cstdint>

namespace sbx::math {

namespace detail {

template<typename Representation>
struct ieee_754_traits;

template<>
struct ieee_754_traits<std::float_t> {
  using bits_type = std::uint32_t;
  inline static constexpr auto bias = bits_type{127};
  inline static constexpr auto mantissa_bits = bits_type{23};
  inline static constexpr auto exponent_bits = bits_type{8};
}; // struct ieee_754_traits<std::float_t>

template<>
struct ieee_754_traits<std::double_t> {
  using bits_type = std::uint64_t;
  inline static constexpr auto bias = bits_type{1023};
  inline static constexpr auto mantissa_bits = bits_type{52};
  inline static constexpr auto exponent_bits = bits_type{11};
}; // struct ieee_754_traits<std::double_t>

} // namespace detail

/**
 * @brief A raw IEEE 754 bit pattern, with sign/exponent/mantissa accessors.
 *
 * @tparam Representation The floating-point type this is a bit pattern of; std::float_t or std::double_t (see ieee_754_float32/ieee_754_float64).
 *
 * @note mantissa()/exponent()/sign() extract via shift and mask on the raw integer value, not a bitfield struct member — a bitfield's mapping onto memory bytes is endianness-dependent, but shifting/masking the already-loaded integer value is not, so this needs no endianness handling at all.
 */
template<typename Representation>
union basic_ieee_754_float {

  using representation_type = Representation;
  using traits_type = detail::ieee_754_traits<Representation>;
  using bits_type = typename traits_type::bits_type;

  inline static constexpr auto bias = traits_type::bias;
  inline static constexpr auto mantissa_bits = traits_type::mantissa_bits;
  inline static constexpr auto exponent_bits = traits_type::exponent_bits;

  representation_type f;
  bits_type i;

  /** @return The mantissa (the low mantissa_bits bits). */
  [[nodiscard]] constexpr auto mantissa() const noexcept -> bits_type {
    return i & ((bits_type{1} << mantissa_bits) - 1u);
  }

  /** @return The exponent (the exponent_bits bits above the mantissa), still biased by +bias. */
  [[nodiscard]] constexpr auto exponent() const noexcept -> bits_type {
    return (i >> mantissa_bits) & ((bits_type{1} << exponent_bits) - 1u);
  }

  /** @return The sign bit (the top bit): 0 for positive, 1 for negative. */
  [[nodiscard]] constexpr auto sign() const noexcept -> bits_type {
    return i >> (mantissa_bits + exponent_bits);
  }

}; // union basic_ieee_754_float

/** @brief A raw IEEE 754 binary32 (float) bit pattern. */
using ieee_754_float32 = basic_ieee_754_float<std::float_t>;

/** @brief A raw IEEE 754 binary64 (double) bit pattern. */
using ieee_754_float64 = basic_ieee_754_float<std::double_t>;

} // namespace sbx::math

#endif // LIBSBX_MATH_IEEE_754_FLOAT_HPP_
