// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_IEEE_754_FLOAT_HPP_
#define LIBSBX_MATH_IEEE_754_FLOAT_HPP_

#include <cmath>
#include <cstdint>
#include <bit>

namespace sbx::math {

union ieee_754_float32 {

  inline static constexpr auto bias = std::uint32_t{127};

  std::float_t f;
  std::uint32_t i;
#if std::endian::native == std::endian::little
  struct {
    std::uint32_t mantissa: 23;
    std::uint32_t exponent: 8;
    std::uint32_t sign: 1;
  };
#else
  struct {
    std::uint32_t sign: 1;
    std::uint32_t exponent: 8;
    std::uint32_t mantissa: 23;
  };
#endif
  
}; // ieee_754_float32

union ieee_754_float64 {

  inline static constexpr auto bias = std::uint32_t{1023};

  std::double_t f;
  std::uint64_t i;
#if std::endian::native == std::endian::little
  struct {
    std::uint64_t mantissa: 23;
    std::uint64_t exponent: 8;
    std::uint64_t sign: 1;
  };
#else
  struct {
    std::uint64_t sign: 1;
    std::uint64_t exponent: 8;
    std::uint64_t mantissa: 23;
  };
#endif
  
}; // ieee_754_float64

} // namespace sbx::math

#endif // LIBSBX_MATH_IEEE_754_FLOAT_HPP_
