// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_ENGINE_CONFIG_HPP_
#define LIBSBX_CORE_ENGINE_CONFIG_HPP_

#include <filesystem>
#include <optional>
#include <string>

#include <libsbx/core/threading_policy.hpp>

namespace sbx::core {

/**
 * @brief Specifies the project to open (or scaffold, if it doesn't exist yet) at engine
 * construction. See @ref engine_config::project.
 */
struct project_config {
  std::filesystem::path root{};
  std::string name{"Untitled"};
}; // struct project_config

/**
 * @brief Engine-wide settings, set once at construction and queryable from anywhere via
 * `engine::config()`. Plain aggregate — free to grow with more fields later.
 */
struct engine_config {
  core::threading_policy threading{threading_policy::multi_threaded};

  /**
   * @brief The project to open at startup. Resolved by `engine`'s own constructor — before
   * any module constructs — so `engine::project()` is available from the very first module
   * onward. Unset models a future projectless/launcher flow; not implemented yet.
   */
  std::optional<project_config> project{};
}; // struct engine_config

} // namespace sbx::core

#endif // LIBSBX_CORE_ENGINE_CONFIG_HPP_
