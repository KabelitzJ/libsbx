// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_HALF_HPP_
#define LIBSBX_MATH_HALF_HPP_

namespace sbx::math {

inline constexpr auto float_to_half(std::float_t value) -> std::uint16_t {
  auto bits = std::bit_cast<std::uint32_t>(value);

  auto sign = (bits >> 16) & 0x8000u;
  auto exponent = static_cast<std::int32_t>((bits >> 23) & 0xFF) - 127 + 15;
  auto mantissa = bits & 0x007FFFFFu;

  if (exponent <= 0) {
    return static_cast<std::uint16_t>(sign);
  }

  if (exponent >= 31) {
    return static_cast<std::uint16_t>(sign | 0x7C00u);
  }

  return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exponent) << 10) | (mantissa >> 13));
}

} // namespace sbx::math

#endif // LIBSBX_MATH_HALF_HPP_