// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_PREFERENCES_HPP_
#define EDITOR_EDITOR_PREFERENCES_HPP_

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>

#include <imgui.h>

#include <libsbx/reflection/annotations.hpp>

namespace editor {

// X/R, Y/G, Z/B, W/A -- shared by the inspector's vector/color field markers, the view gizmo and the transform gizmo.
using axis_palette = std::array<ImU32, 4u>;

enum class [[=sbx::reflection::named]] axis_color_scheme : std::uint8_t {
  muted,
  vivid
}; // enum class axis_color_scheme

inline constexpr auto axis_palette_muted = axis_palette{IM_COL32(219, 61, 61, 255), IM_COL32(90, 191, 90, 255), IM_COL32(64, 120, 219, 255), IM_COL32(140, 140, 140, 255)};

// ImGui's own built-in marker colors (GDefaultRgbaColorMarkers).
inline constexpr auto axis_palette_vivid = axis_palette{IM_COL32(240, 20, 20, 255), IM_COL32(20, 240, 20, 255), IM_COL32(20, 20, 240, 255), IM_COL32(140, 140, 140, 255)};

/** @brief Per-user editor settings (Edit > Preferences) in `.sbx/editor/preferences.yaml`; camera speed and FOV stay in camera.yaml. */
struct editor_preferences {
  axis_color_scheme axis_colors{axis_color_scheme::muted};
  std::float_t translate_snap{1.0f};
  std::float_t rotate_snap{15.0f}; // degrees
  std::float_t scale_snap{0.1f};

  /**
   * @brief Loads the preferences; a missing file or keys mean defaults.
   *
   * @param path The preferences file.
   *
   * @return The preferences.
   */
  [[nodiscard]] static auto load(const std::filesystem::path& path) -> editor_preferences;

  auto save(const std::filesystem::path& path) const -> void;
}; // struct editor_preferences

/**
 * @brief The axis palette the current preferences select.
 *
 * @return The palette.
 */
[[nodiscard]] auto axis_colors() -> const axis_palette&;

} // namespace editor

#endif // EDITOR_EDITOR_PREFERENCES_HPP_
