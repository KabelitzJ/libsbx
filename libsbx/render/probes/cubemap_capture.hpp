// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PROBES_CUBEMAP_CAPTURE_HPP_
#define LIBSBX_RENDER_PROBES_CUBEMAP_CAPTURE_HPP_

#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

#include <libsbx/math/color.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::render {

/** @brief The six faces scene_renderer_module renders a cubemap capture from, in order: +X, -X, +Y, -Y, +Z, -Z. */
inline constexpr auto cubemap_face_count = std::uint32_t{6u};

/**
 * @brief The linear HDR scene seen from one point: six square faces, each a 90 degree perspective view.
 *
 * Face f looks along cubemap_face_forward(f) with cubemap_face_up(f) at the top of the image; pixels are face-major, then rows top-down.
 */
struct cubemap_capture {
  std::uint32_t resolution{0u};
  std::vector<math::color> pixels{};
}; // struct cubemap_capture

using cubemap_capture_callback = std::function<void(cubemap_capture&&)>;

/**
 * @brief The direction a capture face looks along.
 *
 * @param face The face, 0 to 5.
 *
 * @return A unit axis.
 */
[[nodiscard]] auto cubemap_face_forward(std::uint32_t face) -> math::vector3;

/**
 * @brief The direction at the top of a capture face's image.
 *
 * @param face The face, 0 to 5.
 *
 * @return A unit axis perpendicular to the face's forward.
 */
[[nodiscard]] auto cubemap_face_up(std::uint32_t face) -> math::vector3;

/**
 * @brief The view matrix a capture face renders with.
 *
 * @param position The capture point.
 * @param face The face, 0 to 5.
 *
 * @return The world-to-view matrix.
 */
[[nodiscard]] auto cubemap_face_view(const math::vector3& position, std::uint32_t face) -> math::matrix4x4;

/**
 * @brief Decodes an IEEE 754 half-precision float.
 *
 * @param value The half's bits.
 *
 * @return The value as a float, infinities and NaNs included.
 */
[[nodiscard]] auto half_to_float(std::uint16_t value) -> std::float_t;

/**
 * @brief Resamples a capture into an equirectangular image laid out like the engine's environment maps (u from atan2(z, x), v from acos(y)).
 *
 * @param capture The capture to resample.
 * @param width The image width.
 * @param height The image height.
 *
 * @return width * height RGB float triples, rows top-down, bilinearly filtered within each face.
 */
[[nodiscard]] auto cubemap_to_equirect(const cubemap_capture& capture, std::uint32_t width, std::uint32_t height) -> std::vector<std::float_t>;

/**
 * @brief A reflection probe's box frame: the node's rotation and position without its scale, which the box's own extents replace.
 *
 * @param world The probe node's world matrix.
 *
 * @return The box-to-world matrix, orthonormal in its upper 3x3.
 */
[[nodiscard]] auto reflection_probe_box(const math::matrix4x4& world) -> math::matrix4x4;

/**
 * @brief Writes a capture as an equirectangular Radiance .hdr (4 x 2 times its face resolution), the format environment maps import from.
 *
 * @param capture The capture to write.
 * @param path The file to write; its directory is created if missing.
 *
 * @return Whether the file was written.
 */
auto save_cubemap_capture_hdr(const cubemap_capture& capture, const std::filesystem::path& path) -> bool;

} // namespace sbx::render

#endif // LIBSBX_RENDER_PROBES_CUBEMAP_CAPTURE_HPP_
