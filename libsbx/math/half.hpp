// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MATH_HALF_HPP_
#define LIBSBX_MATH_HALF_HPP_

#include <bit>
#include <cmath>
#include <cstdint>

namespace sbx::math {

/**
 * @brief Converts a 32-bit float to an IEEE 754 binary16 (half) bit pattern.
 *
 * @param value The value to convert.
 *
 * @return value, converted to half precision.
 *
 * @note Truncates the mantissa's low 13 bits rather than rounding to nearest — every conversion rounds toward zero instead of round-to-nearest-even.
 */
inline constexpr auto float_to_half(std::float_t value) -> std::uint16_t {
  auto bits = std::bit_cast<std::uint32_t>(value);

  auto sign = (bits >> 16) & 0x8000u;
  auto exponent = static_cast<std::int32_t>((bits >> 23) & 0xFF) - 127 + 15;
  auto mantissa = bits & 0x007FFFFFu;

  if (exponent <= 0) {
    return static_cast<std::uint16_t>(sign);
  }

  if (exponent >= 31) {
    // Original exponent was 255 (float infinity/NaN): preserve NaN-ness instead of collapsing every NaN into infinity — a nonzero mantissa sets the half NaN's own mantissa bit (0x0200), a zero mantissa stays a plain infinity.
    return static_cast<std::uint16_t>(sign | 0x7C00u | (mantissa ? 0x0200u : 0u));
  }

  return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10) | (mantissa >> 13));
}

} // namespace sbx::math

#endif // LIBSBX_MATH_HALF_HPP_
