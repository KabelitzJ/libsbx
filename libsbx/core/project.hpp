// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_PROJECT_HPP_
#define LIBSBX_CORE_PROJECT_HPP_

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sbx::core {

/** @brief How many named layers a project has (0-31) — the same 32-bit budget a @ref sbx::scenes::layer_mask spends one bit per layer on. */
inline constexpr auto layer_count = std::size_t{32u};

/**
 * @brief A project defines where content and the cooked-asset cache live, plus project-wide
 * config. Paths are derived from the root but individually overridable, and persisted to a
 * fixed-name `project.sbxproj` file at the project root — never named after the project itself,
 * so a project can be found (and renamed) without already knowing its name, the same reasoning
 * behind Godot's `project.godot`/Unreal's per-folder `.uproject`.
 *
 * This is a plain value. The engine's notion of the *currently open* project (and the
 * launcher's recents list) lives in @ref projects_module.
 */
class project {

public:

  /** @brief The fixed on-disk filename every project is identified by — see @ref project_file. */
  inline static constexpr auto file_name = std::string_view{"project.sbxproj"};

  /** @brief Bumped whenever the on-disk format changes; written by @ref save, checked by @ref load. */
  inline static constexpr auto current_format_version = std::uint32_t{2u}; // v2: added layers/layer_collision_matrix

  project() {
    _layers[0] = "Default";
    _layer_collision_matrix.fill(0xFFFFFFFFu); // everything collides with everything until the user says otherwise
  }

  explicit project(const std::filesystem::path& root, const std::string& name = "Untitled")
  : project{} {
    _root = root;
    _name = name;
  }

  /** @brief Load a project from its `project.sbxproj` file. Throws if the file is missing, invalid, or from a newer format version than this engine understands. */
  [[nodiscard]] static auto load(const std::filesystem::path& file) -> project;

  /**
   * @brief Load the project rooted at @p root, scaffolding it on disk (`assets/`, `logs/`,
   * `.sbx/library/`, a starter `.gitignore` ignoring `.sbx/`, and the `project.sbxproj` file)
   * if it does not exist yet. Idempotent.
   */
  [[nodiscard]] static auto open_or_create(const std::filesystem::path& root, std::string name) -> project;

  /** @brief Write the project to its @ref project_file. */
  auto save() const -> void;

  auto save(const std::filesystem::path& file) const -> void;

  [[nodiscard]] auto root() const noexcept -> const std::filesystem::path& {
    return _root;
  }

  [[nodiscard]] auto name() const noexcept -> const std::string& {
    return _name;
  }

  /** @brief The fixed `<root>/project.sbxproj` config file — see @ref file_name. */
  [[nodiscard]] auto project_file() const -> std::filesystem::path {
    return _root / file_name;
  }

  /** @brief The source-content root (default `<root>/assets`). */
  [[nodiscard]] auto assets_directory() const -> std::filesystem::path {
    return _root / _assets;
  }

  /** @brief The cooked-asset cache (default `<root>/.sbx/library`). */
  [[nodiscard]] auto library_directory() const -> std::filesystem::path {
    return _root / _library;
  }

  /** @brief The log directory (default `<root>/logs` — visible at the project root, not hidden under `.sbx/`). */
  [[nodiscard]] auto logs_directory() const -> std::filesystem::path {
    return _root / _logs;
  }

  /** @brief Resolve a project-relative path against the root. */
  [[nodiscard]] auto resolve(const std::filesystem::path& relative) const -> std::filesystem::path {
    return _root / relative;
  }

  /** @brief The scene (relative to @ref assets_directory) an application should load on startup, if any. */
  [[nodiscard]] auto startup_scene() const noexcept -> const std::optional<std::filesystem::path>& {
    return _startup_scene;
  }

  auto set_name(std::string name) -> void {
    _name = std::move(name);
  }

  auto set_assets_directory(std::filesystem::path relative) -> void {
    _assets = std::move(relative);
  }

  auto set_library_directory(std::filesystem::path relative) -> void {
    _library = std::move(relative);
  }

  auto set_startup_scene(std::optional<std::filesystem::path> relative) -> void {
    _startup_scene = std::move(relative);
  }

  /** @brief The project's 32 named layers, index 0 ("Default") onward — an empty entry is an unnamed/unused layer. */
  [[nodiscard]] auto layers() const noexcept -> const std::array<std::string, layer_count>& {
    return _layers;
  }

  [[nodiscard]] auto layer_name(std::uint8_t index) const -> const std::string& {
    return _layers.at(index);
  }

  auto set_layer_name(std::uint8_t index, std::string name) -> void {
    _layers.at(index) = std::move(name);
  }

  /** @brief Whether layer @p a and layer @p b are allowed to physically collide at all — checked once per broadphase candidate pair (see physics_module::_generate_candidate_pairs). Always symmetric: only ever set through @ref set_layers_collide. */
  [[nodiscard]] auto layers_collide(std::uint8_t a, std::uint8_t b) const -> bool {
    return (_layer_collision_matrix.at(a) & (std::uint32_t{1u} << b)) != 0u;
  }

  /** @brief Sets whether layer @p a and layer @p b collide, keeping the matrix symmetric (both [a] bit b and [b] bit a are written). */
  auto set_layers_collide(std::uint8_t a, std::uint8_t b, bool collide) -> void {
    auto set_bit = [collide](std::uint32_t& row, std::uint8_t bit) {
      if (collide) {
        row |= (std::uint32_t{1u} << bit);
      } else {
        row &= ~(std::uint32_t{1u} << bit);
      }
    };

    set_bit(_layer_collision_matrix.at(a), b);
    set_bit(_layer_collision_matrix.at(b), a);
  }

private:

  std::filesystem::path _root{};
  std::string _name{"Untitled"};
  std::filesystem::path _assets{"assets"};
  std::filesystem::path _library{".sbx/library"};
  std::filesystem::path _logs{"logs"};
  std::optional<std::filesystem::path> _startup_scene{};

  std::array<std::string, layer_count> _layers{};
  std::array<std::uint32_t, layer_count> _layer_collision_matrix{};

}; // class project

} // namespace sbx::core

#endif // LIBSBX_CORE_PROJECT_HPP_
