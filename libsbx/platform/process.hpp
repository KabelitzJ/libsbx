// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PLATFORM_PROCESS_HPP_
#define LIBSBX_PLATFORM_PROCESS_HPP_

#include <filesystem>
#include <string>
#include <vector>

namespace sbx::platform {

struct spawn_options {
  std::vector<std::string> arguments{};
}; // struct spawn_options

/**
 * @brief Fire-and-forget: spawns @p executable as a new, independent process and returns
 * immediately. Does not wait on or track the child. Logs and returns on failure.
 */
auto spawn(const std::filesystem::path& executable, const spawn_options& options = {}) -> void;

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_PROCESS_HPP_
