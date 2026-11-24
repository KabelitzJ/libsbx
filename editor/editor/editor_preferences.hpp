// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_PREFERENCES_HPP_
#define EDITOR_EDITOR_PREFERENCES_HPP_

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

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

  // The editor UI's size: 0 follows the monitor's content scale (high-DPI screens), otherwise this factor.
  std::float_t interface_scale{0.0f};

  // Most recently opened or saved scenes, newest first, relative to the project's assets directory.
  std::vector<std::string> recent_scenes{};

  // Action id to its shortcuts by name (chord_name), only for actions the user remapped; an empty list means "no shortcut".
  std::map<std::string, std::vector<std::string>> shortcut_overrides{};

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

/**
 * @brief The scale the editor UI should use now: the preference, or the main viewport's DPI scale when it's 0 (never below 1 then).
 *
 * @return The scale factor.
 */
[[nodiscard]] auto effective_interface_scale() -> std::float_t;

} // namespace editor

#endif // EDITOR_EDITOR_PREFERENCES_HPP_
