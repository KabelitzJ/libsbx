// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_USER_DATA_DIRECTORY_HPP_
#define LIBSBX_CORE_USER_DATA_DIRECTORY_HPP_

#include <filesystem>

namespace sbx::core {

/**
 * @brief Where per-user, cross-project engine state lives — today just @ref projects_module's
 * recent-projects list. Deliberately never inside any project (it has to be readable before one
 * is open, e.g. by the launcher). `#ifdef`-branched per OS the same way filesystem_module's
 * dormant `_executable_directory()` is: `%APPDATA%\libsbx` on Windows, `~/Library/Application
 * Support/libsbx` on macOS, `$XDG_CONFIG_HOME/libsbx` (falling back to `~/.config/libsbx`) on
 * Linux. Does not create the directory — callers create it on first write.
 */
[[nodiscard]] auto user_data_directory() -> std::filesystem::path;

/**
 * @brief The current user's home directory (`%USERPROFILE%` on Windows, `$HOME` elsewhere) —
 * unlike @ref user_data_directory, this is the plain home directory itself, not an
 * application-specific subfolder of it. Used to resolve dev-time default project locations
 * (e.g. `<home>/Development/<project>`) without hardcoding a path.
 */
[[nodiscard]] auto user_home_directory() -> std::filesystem::path;

} // namespace sbx::core

#endif // LIBSBX_CORE_USER_DATA_DIRECTORY_HPP_
