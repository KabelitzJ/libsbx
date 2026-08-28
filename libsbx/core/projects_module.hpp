// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_PROJECTS_MODULE_HPP_
#define LIBSBX_CORE_PROJECTS_MODULE_HPP_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/project.hpp>

namespace sbx::core {

/** @brief One entry in @ref projects_module's recent-projects list — display data only, not a loaded project. */
struct recent_project_entry {
  std::filesystem::path file{};
  std::string name{};
  std::int64_t last_opened{0}; // seconds since the Unix epoch.
}; // struct recent_project_entry

/**
 * @brief Owns the recent-projects list (the role @ref project.hpp's doc comment already names)
 * and the open/create entry points that keep it up to date. Persisted outside any project, under
 * @ref user_data_directory, so it's readable before one is open — the launcher's whole reason to
 * exist. No `dependencies`: usable by `launcher` and (optionally) `editor` alike.
 */
class projects_module final : public utility::noncopyable {

public:

  projects_module();

  ~projects_module() = default;

  /**
   * @brief Loads and activates the project at @p path — either its `project.sbxproj` file, or
   * its root directory — and records it as most-recently-opened. Throws if it doesn't exist
   * (unlike @ref create, this never scaffolds one).
   */
  auto open(const std::filesystem::path& path) -> core::project&;

  /** @brief Scaffolds (if needed) and activates the project rooted at @p root, and records it as most-recently-opened. */
  auto create(const std::filesystem::path& root, std::string name) -> core::project&;

  /** @brief Drops @p file from the recent-projects list. Does not touch the project on disk. */
  auto remove_recent(const std::filesystem::path& file) -> void;

  /** @brief Most-recently-opened first. */
  [[nodiscard]] auto recent_projects() const noexcept -> const std::vector<recent_project_entry>& {
    return _recents;
  }

private:

  [[nodiscard]] auto _recents_file() const -> std::filesystem::path;

  auto _load_recents() -> void;

  auto _save_recents() const -> void;

  /** @brief Moves (or inserts) @p project to the front of the recent-projects list and persists it. */
  auto _touch_recent(const core::project& project) -> void;

  std::vector<recent_project_entry> _recents{};

}; // class projects_module

} // namespace sbx::core

#endif // LIBSBX_CORE_PROJECTS_MODULE_HPP_
