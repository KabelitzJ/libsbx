// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_SCENES_POST_PROCESS_HPP_
#define LIBSBX_SCENES_POST_PROCESS_HPP_

#include <cmath>
#include <cstdint>

#include <libsbx/math/color.hpp>

#include <libsbx/assets/texture2d.hpp>

namespace sbx::scenes {

/**
 * @brief Everything the renderer does to a camera's lit image before the UI, one section per
 * effect. New effects (depth of field, colour grading, fog, ambient occlusion) each get their own
 * section here, off or neutral by default, so a scene only ever gets what it asks for -- a game's
 * look is data (this struct, and whatever script drives it), never baked into the engine.
 *
 * Bloom keeps its long-standing defaults (on), so existing scenes look the same.
 */
struct post_process_settings {
  struct bloom_settings {
    bool enabled{true};
    std::float_t intensity{0.04f};
    std::float_t threshold{1.0f};
    std::float_t knee{0.1f};
  }; // struct bloom_settings

  /**
   * @brief Blur by focus, gathered in tonemap_pass. distance: sharp within focus_distance +- half
   * focus_range (world units from the camera), fully blurred another focus_range further. screen_band:
   * sharp within band_center +- half band_height (fraction of the screen height from the top), fully
   * blurred another band_height out -- a tilt-shift "miniature" look, independent of depth.
   * max_blur is the full blur's radius as a fraction of the screen height.
   */
  struct depth_of_field_settings {
    enum class focus_mode : std::uint8_t {
      distance,
      screen_band
    }; // enum class focus_mode

    bool enabled{false};
    focus_mode mode{focus_mode::distance};
    std::float_t focus_distance{10.0f};
    std::float_t focus_range{10.0f};
    std::float_t band_center{0.5f};
    std::float_t band_height{0.3f};
    std::float_t max_blur{0.008f};
    std::uint32_t samples{16u};
  }; // struct depth_of_field_settings

  /**
   * @brief Grading after tonemapping, in display (sRGB) space: contrast and saturation, then an
   * optional lookup table -- a strip of size x size slices, size*size wide and size tall, red
   * along x within a slice, green down y, blue across slices; loaded unorm (it's data, not colour).
   * No lookup table (and 1 / 1) = unchanged.
   */
  struct color_grading_settings {
    assets::texture2d_handle lut{};
    std::float_t lut_contribution{1.0f};
    std::float_t contrast{1.0f};
    std::float_t saturation{1.0f};
  }; // struct color_grading_settings

  /**
   * @brief Exponential height fog, applied in tonemap_pass from the depth buffer: density per world
   * unit at base_height, thinning by e per 1 / height_falloff units above it (0 = the same at every
   * height), starting start units from the camera, never more opaque than max_opacity. color is
   * linear. The sky (nothing in the depth buffer) only gets it with affects_sky.
   */
  struct fog_settings {
    bool enabled{false};
    math::color color{0.6f, 0.7f, 0.8f, 1.0f};
    std::float_t density{0.01f};
    std::float_t start{0.0f};
    std::float_t height_falloff{0.0f};
    std::float_t base_height{0.0f};
    std::float_t max_opacity{1.0f};
    bool affects_sky{false};
  }; // struct fog_settings

  /**
   * @brief Screen-space ambient occlusion (ambient_occlusion_pass, from the depth buffer at half
   * resolution): darkens ambient and environment light where nearby geometry blocks it -- creases,
   * contact, under trees. radius is the reach in world units; intensity scales the darkening.
   * Direct light is untouched.
   */
  struct ambient_occlusion_settings {
    bool enabled{false};
    std::float_t radius{1.0f};
    std::float_t intensity{1.0f};
    std::uint32_t samples{8u};
  }; // struct ambient_occlusion_settings

  std::float_t exposure{0.0f}; // EV stops applied as exp2(exposure) before tonemapping; 0 = unchanged.
  bloom_settings bloom{};
  ambient_occlusion_settings ambient_occlusion{};
  fog_settings fog{};
  depth_of_field_settings depth_of_field{};
  color_grading_settings color_grading{};
}; // struct post_process_settings

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_POST_PROCESS_HPP_
