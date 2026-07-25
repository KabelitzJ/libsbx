// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_PROJECT_HPP_
#define LIBSBX_CORE_PROJECT_HPP_

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace sbx::core {

/**
 * @brief A project defines where content and the cooked-asset cache live, plus project-wide config.
 * Paths are derived from the root but individually overridable, and persisted to a
 * `project.sbxproject` file at the project root.
 *
 * This is a plain value. The engine's notion of the *currently open* project (and the
 * launcher's recents list) lives in @ref projects_module.
 */
class project {

public:

  inline static constexpr auto file_extension = std::string_view{".sbxproj"};

  project() = default;

  explicit project(const std::filesystem::path& root, const std::string& name = "Untitled")
  : _root{root}, _name{name} { }

  /** @brief Load a project from a `.sbxproject` file. Throws if the file is missing or invalid. */
  [[nodiscard]] static auto load(const std::filesystem::path& file) -> project;

  /**
   * @brief Load the project rooted at @p root, scaffolding it on disk (content and library
   * directories plus the `project.sbxproject` file) if it does not exist yet. Idempotent.
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

  /** @brief The `<root>/project.sbxproject` config file. */
  [[nodiscard]] auto project_file() const -> std::filesystem::path;

  /** @brief The source-content root (default `<root>/assets`). */
  [[nodiscard]] auto assets_directory() const -> std::filesystem::path {
    return _root / _assets;
  }

  /** @brief The cooked-asset cache (default `<root>/.sbx/library`). */
  [[nodiscard]] auto library_directory() const -> std::filesystem::path {
    return _root / _library;
  }

  /** @brief Resolve a project-relative path against the root. */
  [[nodiscard]] auto resolve(const std::filesystem::path& relative) const -> std::filesystem::path {
    return _root / relative;
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

private:

  std::filesystem::path _root{};
  std::string _name{"Untitled"};
  std::filesystem::path _assets{"assets"};
  std::filesystem::path _library{".sbx/library"};

}; // class project

} // namespace sbx::core

#endif // LIBSBX_CORE_PROJECT_HPP_
