// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CORE_THREADING_POLICY_HPP_
#define LIBSBX_CORE_THREADING_POLICY_HPP_

#include <cstdint>

namespace sbx::core {

/**
 * @brief How the engine's render pipeline is driven.
 *
 * `single_threaded` runs the frame inline on the calling thread. No background thread, no cross-thread resource lifetime hazards. 
 * `multi_threaded` owns a dedicated render thread and hands frames off to it.
 */
enum class threading_policy : std::uint8_t {
  single_threaded,
  multi_threaded
}; // enum class threading_policy

} // namespace sbx::core

#endif // LIBSBX_CORE_THREADING_POLICY_HPP_
