// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_ENGINE_CONFIG_HPP_
#define LIBSBX_CORE_ENGINE_CONFIG_HPP_

#include <libsbx/core/threading_policy.hpp>

namespace sbx::core {

/**
 * @brief Engine-wide settings, set once at construction and queryable from anywhere via
 * `engine::config()`. Plain aggregate — free to grow with more fields later.
 */
struct engine_config {
  core::threading_policy threading{threading_policy::multi_threaded};
}; // struct engine_config

} // namespace sbx::core

#endif // LIBSBX_CORE_ENGINE_CONFIG_HPP_
