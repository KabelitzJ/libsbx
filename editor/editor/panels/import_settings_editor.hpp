// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_IMPORT_SETTINGS_EDITOR_HPP_
#define EDITOR_PANELS_IMPORT_SETTINGS_EDITOR_HPP_

#include <filesystem>
#include <optional>
#include <vector>

#include <libsbx/math/uuid.hpp>

#include <libsbx/assets/asset_type.hpp>
#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/assets/import_settings.hpp>

namespace editor {

/**
 * @brief A working copy of one asset's importer settings, shared by the import window and the Inspector's Import Settings section.
 *
 * Edits stay local until apply(), so both places get Revert for free.
 */
class import_settings_editor {

public:

  import_settings_editor() = default;

  /**
   * @brief Opens the editor on an asset file; reads its `.meta` settings if it has one, otherwise the type's defaults.
   *
   * @param relative_path The asset, relative to the assets directory.
   */
  explicit import_settings_editor(std::filesystem::path relative_path);

  /**
   * @brief Draws the type's settings UI.
   *
   * @return True if a value changed this frame.
   */
  auto draw() -> bool;

  /** @brief Imports the file if it isn't yet, writes the settings into its `.meta` and re-cooks it so live handles update. */
  auto apply() -> sbx::math::uuid;

  auto revert() -> void {
    _settings = _applied;
    _sync_checks_from_settings();
  }

  [[nodiscard]] auto is_dirty() const -> bool {
    return _settings != _applied;
  }

  [[nodiscard]] auto type() const noexcept -> sbx::assets::asset_type {
    return _type;
  }

  [[nodiscard]] auto relative_path() const noexcept -> const std::filesystem::path& {
    return _relative_path;
  }

  [[nodiscard]] auto settings() const noexcept -> const sbx::assets::import_settings& {
    return _settings;
  }

  /**
   * @brief Adopts another file's settings, for "apply to all remaining"; per-file selections (mesh primitives and clips) reset to everything.
   *
   * @param settings The settings to copy.
   */
  auto adopt(const sbx::assets::import_settings& settings) -> void;

private:

  auto _draw_texture(sbx::assets::texture_import_settings& settings) -> bool;
  auto _draw_mesh(sbx::assets::mesh_import_options& settings) -> bool;
  auto _draw_font(sbx::assets::font_import_settings& settings) -> bool;

  auto _sync_checks_from_settings() -> void;

  std::filesystem::path _relative_path{};
  sbx::assets::asset_type _type{sbx::assets::asset_type::unknown};
  sbx::math::uuid _id{sbx::math::uuid::nil()};

  sbx::assets::import_settings _settings{};
  sbx::assets::import_settings _applied{};

  std::optional<sbx::assets::mesh_source_summary> _mesh_summary{};
  std::vector<bool> _primitive_checks{};
  std::vector<bool> _animation_checks{};

}; // class import_settings_editor

} // namespace editor

#endif // EDITOR_PANELS_IMPORT_SETTINGS_EDITOR_HPP_
