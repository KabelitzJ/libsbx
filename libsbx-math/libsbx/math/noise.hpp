// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_NOISE_HPP_
#define LIBSBX_MATH_NOISE_HPP_

#include <array>
#include <cinttypes>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/random.hpp>
#include <libsbx/math/concepts.hpp>

namespace sbx::math {

class noise {
 
  inline static auto permutation = std::array<std::uint8_t, 256>{
    151, 160, 137, 91, 90, 15,
    131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
    190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
    77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
    102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
    135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
    5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
    223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
    129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
    251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
    49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
  };

  inline static constexpr auto F2 = 0.366025403f;
  inline static constexpr auto G2 = 0.211324865f;

public:

  static constexpr auto fractal(const std::float_t x, const std::float_t y, std::uint32_t octaves) -> std::float_t {
    auto output = 0.f;
    auto denom  = 0.f;
    auto frequency = 1.0f;
    auto amplitude = 1.0f;

    for (auto i = 0u; i < octaves; i++) {
      output += (amplitude * simplex(x * frequency, y * frequency));
      denom += amplitude;

      frequency *= 2.0f;
      amplitude *= 0.5f;
    }

    return (output / denom);
  }

  static constexpr auto fractal(const sbx::math::vector3& vector, std::uint32_t octaves) -> std::float_t {
    return fractal(vector.x(), vector.y(), vector.z(), octaves);
  }

  static constexpr auto fractal(const std::float_t x, const std::float_t y, const std::float_t z, std::uint32_t octaves) -> std::float_t {
    auto output = 0.f;
    auto denom  = 0.f;
    auto frequency = 1.0f;
    auto amplitude = 1.0f;

    for (auto i = 0u; i < octaves; i++) {
      output += (amplitude * simplex(x * frequency, y * frequency, z * frequency));
      denom += amplitude;

      frequency *= 2.0f;
      amplitude *= 0.5f;
    }

    return (output / denom);
  }

  static constexpr auto simplex(const std::float_t x, const std::float_t y) -> std::float_t {
    auto n0 = 0.0f;
    auto n1 = 0.0f; 
    auto n2 = 0.0f;

    const auto s = (x + y) * F2;
    const auto xs = x + s;
    const auto ys = y + s;
    const auto i = fast_floor(xs);
    const auto j = fast_floor(ys);

    const auto t = static_cast<std::float_t>(i + j) * G2;
    const auto X0 = static_cast<std::float_t>(i) - t;
    const auto Y0 = static_cast<std::float_t>(j) - t;
    const auto x0 = x - X0;
    const auto y0 = y - Y0;

    auto i1 = 0.0f;
    auto j1 = 0.0f;

    if (x0 > y0) {
      i1 = 1.0f;
      j1 = 0.0f;
    } else {
      i1 = 0.0f;
      j1 = 1.0f;
    }

    const auto x1 = x0 - i1 + G2;
    const auto y1 = y0 - j1 + G2;
    const auto x2 = x0 - 1.0f + 2.0f * G2;
    const auto y2 = y0 - 1.0f + 2.0f * G2;

    const auto gi0 = hash(i + hash(j));
    const auto gi1 = hash(i + i1 + hash(j + j1));
    const auto gi2 = hash(i + 1 + hash(j + 1));

    auto t0 = 0.5f - x0 * x0 - y0 * y0;
    
    if (t0 < 0.0f) {
      n0 = 0.0f;
    } else {
      t0 *= t0;
      n0 = t0 * t0 * grad(gi0, x0, y0);
    }

    auto t1 = 0.5f - x1 * x1 - y1 * y1;

    if (t1 < 0.0f) {
      n1 = 0.0f;
    } else {
      t1 *= t1;
      n1 = t1 * t1 * grad(gi1, x1, y1);
    }

    auto t2 = 0.5f - x2 * x2 - y2 * y2;

    if (t2 < 0.0f) {
      n2 = 0.0f;
    } else {
      t2 *= t2;
      n2 = t2 * t2 * grad(gi2, x2, y2);
    }

    return 45.23065f * (n0 + n1 + n2);
  }

  static constexpr auto simplex(const std::float_t x, const std::float_t y, const std::float_t z) -> std::float_t {
    auto n0 = 0.0f;
    auto n1 = 0.0f; 
    auto n2 = 0.0f;
    auto n3 = 0.0f; // Noise contributions from the four corners

    // Skewing/Unskewing factors for 3D
    constexpr auto F3 = 1.0f / 3.0f;
    constexpr auto G3 = 1.0f / 6.0f;

    // Skew the input space to determine which simplex cell we're in
    auto s = (x + y + z) * F3; // Very nice and simple skew factor for 3D
    auto i = fast_floor(x + s);
    auto j = fast_floor(y + s);
    auto k = fast_floor(z + s);
    auto t = (i + j + k) * G3;
    auto X0 = i - t; // Unskew the cell origin back to (x,y,z) space
    auto Y0 = j - t;
    auto Z0 = k - t;
    auto x0 = x - X0; // The x,y,z distances from the cell origin
    auto y0 = y - Y0;
    auto z0 = z - Z0;

    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
    auto i1 = 0.0f; 
    auto j1 = 0.0f; 
    auto k1 = 0.0f; // Offsets for second corner of simplex in (i,j,k) coords
    auto i2 = 0.0f; 
    auto j2 = 0.0f; 
    auto k2 = 0.0f; // Offsets for third corner of simplex in (i,j,k) coords

    if (x0 >= y0) {
      if (y0 >= z0) {
        i1 = 1; 
        j1 = 0; 
        k1 = 0; 
        i2 = 1; 
        j2 = 1; 
        k2 = 0; // X Y Z order
      } else if (x0 >= z0) {
        i1 = 1; 
        j1 = 0; 
        k1 = 0; 
        i2 = 1; 
        j2 = 0; 
        k2 = 1; // X Z Y order
      } else {
        i1 = 0; 
        j1 = 0; 
        k1 = 1; 
        i2 = 1; 
        j2 = 0; 
        k2 = 1; // Z X Y order
      }
    } else { // x0<y0
      if (y0 < z0) {
        i1 = 0; 
        j1 = 0; 
        k1 = 1; 
        i2 = 0; 
        j2 = 1; 
        k2 = 1; // Z Y X order
      } else if (x0 < z0) {
        i1 = 0; 
        j1 = 1; 
        k1 = 0; 
        i2 = 0; 
        j2 = 1; 
        k2 = 1; // Y Z X order
      } else {
        i1 = 0; 
        j1 = 1; 
        k1 = 0; 
        i2 = 1; 
        j2 = 1; 
        k2 = 0; // Y X Z order
      }
    }

    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.
    auto x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords
    auto y1 = y0 - j1 + G3;
    auto z1 = z0 - k1 + G3;
    auto x2 = x0 - i2 + 2.0f * G3; // Offsets for third corner in (x,y,z) coords
    auto y2 = y0 - j2 + 2.0f * G3;
    auto z2 = z0 - k2 + 2.0f * G3;
    auto x3 = x0 - 1.0f + 3.0f * G3; // Offsets for last corner in (x,y,z) coords
    auto y3 = y0 - 1.0f + 3.0f * G3;
    auto z3 = z0 - 1.0f + 3.0f * G3;

    // Work out the hashed gradient indices of the four simplex corners
    auto gi0 = hash(i + hash(j + hash(k)));
    auto gi1 = hash(i + i1 + hash(j + j1 + hash(k + k1)));
    auto gi2 = hash(i + i2 + hash(j + j2 + hash(k + k2)));
    auto gi3 = hash(i + 1 + hash(j + 1 + hash(k + 1)));

    // Calculate the contribution from the four corners
    auto t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;

    if (t0 < 0) {
      n0 = 0.0;
    } else {
      t0 *= t0;
      n0 = t0 * t0 * grad(gi0, x0, y0, z0);
    }

    auto t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;

    if (t1 < 0) {
      n1 = 0.0;
    } else {
      t1 *= t1;
      n1 = t1 * t1 * grad(gi1, x1, y1, z1);
    }

    auto t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;

    if (t2 < 0) {
      n2 = 0.0;
    } else {
      t2 *= t2;
      n2 = t2 * t2 * grad(gi2, x2, y2, z2);
    }

    auto t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;

    if (t3 < 0) {
      n3 = 0.0;
    } else {
      t3 *= t3;
      n3 = t3 * t3 * grad(gi3, x3, y3, z3);
    }

    // Add contributions from each corner to get the final noise value.
    // The result is scaled to stay just inside [-1,1]
    return 32.0f * (n0 + n1 + n2 + n3);
  }

private:

  static constexpr auto fast_floor(std::float_t fp) -> std::int32_t {
    auto i = static_cast<std::int32_t>(fp);
    return (fp < i) ? (i - 1) : (i);
  }

  template<typename Type>
  static constexpr auto hash(Type i) -> std::uint8_t {
    return permutation[static_cast<std::uint8_t>(i)];
  }

  static constexpr auto grad(std::int32_t hash, std::float_t x, std::float_t y) -> std::float_t {
    const auto h = hash & 0x3F;
    const auto u = h < 4 ? x : y;
    const auto v = h < 4 ? y : x;

    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
  }

  static constexpr float grad(std::int32_t hash, std::float_t x, std::float_t y, std::float_t z) {
    const auto h = hash & 15;
    const auto u = h < 8 ? x : y; 
    const auto v = h < 4 ? y : h == 12 || h == 14 ? x : z;

    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
  }

}; // class noise

} // namespace sbx::math

#endif // LIBSBX_MATH_NOISE_HPP_
