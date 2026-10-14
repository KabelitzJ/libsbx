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
#include <vector>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::core {

/** @brief How many named layers a project has (0-31) — the same 32-bit budget a @ref sbx::scenes::layer_mask spends one bit per layer on. */
inline constexpr auto layer_count = std::size_t{32u};

/** @brief The root of the mixer group tree. A fixed id so scenes can reference it before the project is ever saved. */
inline constexpr auto master_mixer_group_id = math::uuid::from_value(1u);

/** @brief One row of the project's scene list (Unity's Build Settings). */
struct scene_list_entry {
  std::filesystem::path path{}; // relative to the assets directory
  bool enabled{true};
}; // struct scene_list_entry

/** @brief One node of the project's audio mixer tree; a source's gain is multiplied by every group up to Master. */
struct mixer_group {
  math::uuid id{math::uuid::nil()};
  std::string name{};
  math::uuid parent{math::uuid::nil()}; // nil only for Master
  std::float_t volume{1.0f};
  bool muted{false};
}; // struct mixer_group

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

  inline static constexpr auto default_fixed_timestep = std::float_t{1.0f / 60.0f};

  inline static constexpr auto default_max_voices = std::uint32_t{64u};
  inline static constexpr auto default_msaa_samples = std::uint32_t{4u};

  /** @brief Bumped whenever the on-disk format changes; written by @ref save, checked by @ref load. */
  inline static constexpr auto current_format_version = std::uint32_t{6u}; // v2: added layers/layer_collision_matrix, v3: added physics/time, v4: added audio, v5: scene list replaces startup_scene, v6: added rendering

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

  /** @brief The game's scenes in build order; disabled entries are kept but don't count. */
  [[nodiscard]] auto scene_list() const noexcept -> const std::vector<scene_list_entry>& {
    return _scene_list;
  }

  auto set_scene_list(std::vector<scene_list_entry> list) -> void {
    _scene_list = std::move(list);
  }

  /** @brief The scene to open on start: build index 0, the first enabled entry. */
  [[nodiscard]] auto startup_scene() const -> std::optional<std::filesystem::path> {
    return scene_at(0u);
  }

  /** @brief How many scenes have a build index (enabled entries). */
  [[nodiscard]] auto scene_count_in_build() const -> std::size_t;

  /** @brief The enabled scene at @p build_index. */
  [[nodiscard]] auto scene_at(std::size_t build_index) const -> std::optional<std::filesystem::path>;

  /** @brief The build index of an enabled scene, by its assets-relative path. */
  [[nodiscard]] auto build_index_of(const std::filesystem::path& path) const -> std::optional<std::size_t>;

  /**
   * @brief An enabled scene by name or path, like Unity's LoadScene(string): a bare name matches the file name without extension, case-insensitively;
   * anything with a '/' or the .scene extension matches the assets-relative path (the extension may be omitted).
   */
  [[nodiscard]] auto resolve_scene(std::string_view name_or_path) const -> std::optional<std::filesystem::path>;

  auto set_name(std::string name) -> void {
    _name = std::move(name);
  }

  auto set_assets_directory(std::filesystem::path relative) -> void {
    _assets = std::move(relative);
  }

  auto set_library_directory(std::filesystem::path relative) -> void {
    _library = std::move(relative);
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

  [[nodiscard]] auto gravity() const noexcept -> const math::vector3& {
    return _gravity;
  }

  auto set_gravity(const math::vector3& gravity) noexcept -> void {
    _gravity = gravity;
  }

  /** @brief Contact solver iterations per physics step -- more is stiffer stacking, at more CPU cost. */
  [[nodiscard]] auto velocity_iterations() const noexcept -> std::uint32_t {
    return _velocity_iterations;
  }

  auto set_velocity_iterations(std::uint32_t iterations) noexcept -> void {
    _velocity_iterations = iterations;
  }

  /** @brief Seconds per fixed_update tick (physics, script FixedUpdate) -- see engine::fixed_delta_time. */
  [[nodiscard]] auto fixed_timestep() const noexcept -> std::float_t {
    return _fixed_timestep;
  }

  auto set_fixed_timestep(std::float_t seconds) noexcept -> void {
    _fixed_timestep = seconds;
  }

  /** @brief The mixer tree, Master first. Always contains Master, and every other group's parent exists. */
  [[nodiscard]] auto mixer_groups() const noexcept -> const std::vector<mixer_group>& {
    return _mixer_groups;
  }

  /** @brief Replaces the mixer tree, repairing it: Master is restored if missing and orphans are reparented to it. */
  auto set_mixer_groups(std::vector<mixer_group> groups) -> void;

  /** @brief How many sounds can be audible at once; beyond it, the least important are virtualized. */
  [[nodiscard]] auto max_voices() const noexcept -> std::uint32_t {
    return _max_voices;
  }

  auto set_max_voices(std::uint32_t count) noexcept -> void {
    _max_voices = count;
  }

  /** @brief MSAA samples per pixel (1 = off, 2, 4 or 8); the renderer clamps it to what the device supports. */
  [[nodiscard]] auto msaa_samples() const noexcept -> std::uint32_t {
    return _msaa_samples;
  }

  auto set_msaa_samples(std::uint32_t samples) noexcept -> void {
    _msaa_samples = samples;
  }

private:

  std::filesystem::path _root{};
  std::string _name{"Untitled"};
  std::filesystem::path _assets{"assets"};
  std::filesystem::path _library{".sbx/library"};
  std::filesystem::path _logs{"logs"};
  std::vector<scene_list_entry> _scene_list{};

  std::array<std::string, layer_count> _layers{};
  std::array<std::uint32_t, layer_count> _layer_collision_matrix{};

  math::vector3 _gravity{0.0f, -9.81f, 0.0f};
  std::uint32_t _velocity_iterations{8u};
  std::float_t _fixed_timestep{default_fixed_timestep};

  std::vector<mixer_group> _mixer_groups{
    mixer_group{master_mixer_group_id, "Master"},
    mixer_group{math::uuid::from_value(2u), "Music", master_mixer_group_id},
    mixer_group{math::uuid::from_value(3u), "SFX", master_mixer_group_id},
    mixer_group{math::uuid::from_value(4u), "UI", master_mixer_group_id},
    mixer_group{math::uuid::from_value(5u), "Voice", master_mixer_group_id}
  };
  std::uint32_t _max_voices{default_max_voices};

  std::uint32_t _msaa_samples{default_msaa_samples};

}; // class project

} // namespace sbx::core

#endif // LIBSBX_CORE_PROJECT_HPP_
