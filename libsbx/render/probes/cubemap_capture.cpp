// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/probes/cubemap_capture.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <system_error>

#include <stb_image_write.h>

#include <libsbx/math/constants.hpp>

namespace sbx::render {

auto cubemap_face_forward(std::uint32_t face) -> math::vector3 {
  static const auto forwards = std::array<math::vector3, cubemap_face_count>{
    math::vector3{1.0f, 0.0f, 0.0f},
    math::vector3{-1.0f, 0.0f, 0.0f},
    math::vector3{0.0f, 1.0f, 0.0f},
    math::vector3{0.0f, -1.0f, 0.0f},
    math::vector3{0.0f, 0.0f, 1.0f},
    math::vector3{0.0f, 0.0f, -1.0f}
  };

  return forwards[face % cubemap_face_count];
}

auto cubemap_face_up(std::uint32_t face) -> math::vector3 {
  static const auto ups = std::array<math::vector3, cubemap_face_count>{
    math::vector3{0.0f, 1.0f, 0.0f},
    math::vector3{0.0f, 1.0f, 0.0f},
    math::vector3{0.0f, 0.0f, -1.0f},
    math::vector3{0.0f, 0.0f, 1.0f},
    math::vector3{0.0f, 1.0f, 0.0f},
    math::vector3{0.0f, 1.0f, 0.0f}
  };

  return ups[face % cubemap_face_count];
}

auto cubemap_face_view(const math::vector3& position, std::uint32_t face) -> math::matrix4x4 {
  return math::matrix4x4::look_at(position, position + cubemap_face_forward(face), cubemap_face_up(face));
}

auto half_to_float(std::uint16_t value) -> std::float_t {
  const auto sign = static_cast<std::uint32_t>(value & 0x8000u) << 16u;
  const auto exponent = static_cast<std::uint32_t>(value >> 10u) & 0x1Fu;
  auto mantissa = static_cast<std::uint32_t>(value) & 0x3FFu;

  if (exponent == 0u) {
    if (mantissa == 0u) {
      return std::bit_cast<std::float_t>(sign);
    }

    // Subnormal: shift the mantissa up until its leading one becomes the implicit bit.
    auto shift = std::int32_t{0};

    while ((mantissa & 0x400u) == 0u) {
      mantissa <<= 1u;
      ++shift;
    }

    mantissa &= 0x3FFu;

    return std::bit_cast<std::float_t>(sign | static_cast<std::uint32_t>(127 - 15 + 1 - shift) << 23u | mantissa << 13u);
  }

  if (exponent == 0x1Fu) {
    return std::bit_cast<std::float_t>(sign | 0x7F800000u | mantissa << 13u);
  }

  return std::bit_cast<std::float_t>(sign | (exponent + 127u - 15u) << 23u | mantissa << 13u);
}

auto cubemap_to_equirect(const cubemap_capture& capture, std::uint32_t width, std::uint32_t height) -> std::vector<std::float_t> {
  auto result = std::vector<std::float_t>(static_cast<std::size_t>(width) * height * 3u, 0.0f);

  const auto resolution = capture.resolution;

  if (resolution == 0u || capture.pixels.size() < static_cast<std::size_t>(resolution) * resolution * cubemap_face_count) {
    return result;
  }

  const auto texel = [&](std::uint32_t face, std::int32_t x, std::int32_t y) -> const math::color& {
    const auto clamped_x = static_cast<std::size_t>(std::clamp(x, 0, static_cast<std::int32_t>(resolution) - 1));
    const auto clamped_y = static_cast<std::size_t>(std::clamp(y, 0, static_cast<std::int32_t>(resolution) - 1));

    return capture.pixels[(static_cast<std::size_t>(face) * resolution + clamped_y) * resolution + clamped_x];
  };

  for (auto row = std::uint32_t{0u}; row < height; ++row) {
    // The inverse of direction_to_equirect (common.slang).
    const auto theta = (static_cast<std::float_t>(row) + 0.5f) / static_cast<std::float_t>(height) * math::pif;

    for (auto column = std::uint32_t{0u}; column < width; ++column) {
      const auto phi = ((static_cast<std::float_t>(column) + 0.5f) / static_cast<std::float_t>(width) - 0.5f) * 2.0f * math::pif;
      const auto direction = math::vector3{std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};

      // The face the direction leaves through, and where on its image it lands.
      auto face = std::uint32_t{0u};
      auto facing = -1.0f;

      for (auto candidate = std::uint32_t{0u}; candidate < cubemap_face_count; ++candidate) {
        const auto dot = math::vector3::dot(direction, cubemap_face_forward(candidate));

        if (dot > facing) {
          facing = dot;
          face = candidate;
        }
      }

      const auto forward = cubemap_face_forward(face);
      const auto up = cubemap_face_up(face);
      const auto right = math::vector3::cross(forward, up);

      const auto ndc_x = math::vector3::dot(direction, right) / facing;
      const auto ndc_y = math::vector3::dot(direction, up) / facing;

      const auto x = (ndc_x * 0.5f + 0.5f) * static_cast<std::float_t>(resolution) - 0.5f;
      const auto y = (0.5f - ndc_y * 0.5f) * static_cast<std::float_t>(resolution) - 0.5f;

      const auto x0 = static_cast<std::int32_t>(std::floor(x));
      const auto y0 = static_cast<std::int32_t>(std::floor(y));
      const auto fx = x - static_cast<std::float_t>(x0);
      const auto fy = y - static_cast<std::float_t>(y0);

      const auto& c00 = texel(face, x0, y0);
      const auto& c10 = texel(face, x0 + 1, y0);
      const auto& c01 = texel(face, x0, y0 + 1);
      const auto& c11 = texel(face, x0 + 1, y0 + 1);

      const auto mix = [&](std::float_t a, std::float_t b, std::float_t c, std::float_t d) {
        return (a * (1.0f - fx) + b * fx) * (1.0f - fy) + (c * (1.0f - fx) + d * fx) * fy;
      };

      auto* out = result.data() + (static_cast<std::size_t>(row) * width + column) * 3u;
      out[0] = mix(c00.r(), c10.r(), c01.r(), c11.r());
      out[1] = mix(c00.g(), c10.g(), c01.g(), c11.g());
      out[2] = mix(c00.b(), c10.b(), c01.b(), c11.b());
    }
  }

  return result;
}

auto save_cubemap_capture_hdr(const cubemap_capture& capture, const std::filesystem::path& path) -> bool {
  const auto width = capture.resolution * 4u;
  const auto height = capture.resolution * 2u;

  if (width == 0u) {
    return false;
  }

  const auto pixels = cubemap_to_equirect(capture, width, height);

  auto error = std::error_code{};
  std::filesystem::create_directories(path.parent_path(), error);

  return stbi_write_hdr(path.string().c_str(), static_cast<int>(width), static_cast<int>(height), 3, pixels.data()) != 0;
}

auto reflection_probe_box(const math::matrix4x4& world) -> math::matrix4x4 {
  auto box = world;

  for (auto column = std::size_t{0u}; column < 3u; ++column) {
    const auto axis = math::vector3{world[column]};
    const auto length = axis.length();

    box[column] = math::vector4{length > 0.0f ? axis / length : math::vector3{column == 0u ? 1.0f : 0.0f, column == 1u ? 1.0f : 0.0f, column == 2u ? 1.0f : 0.0f}, 0.0f};
  }

  return box;
}

} // namespace sbx::render
