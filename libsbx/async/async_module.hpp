// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASYNC_ASYNC_MODULE_HPP_
#define LIBSBX_ASYNC_ASYNC_MODULE_HPP_

#include <cstdint>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/async/worker_pool.hpp>

namespace sbx::async {

/**
 * @brief The engine's job system: a worker_pool sized from engine_config::worker_count, or by default one worker per core
 * left over after the main thread (and the render thread, when the threading policy runs one). See worker_pool for the rules jobs follow.
 */
class async_module final : public utility::noncopyable {

public:

  async_module();

  ~async_module() = default;

  [[nodiscard]] auto pool() noexcept -> worker_pool& {
    return _pool;
  }

private:

  worker_pool _pool;

}; // class async_module

} // namespace sbx::async

#endif // LIBSBX_ASYNC_ASYNC_MODULE_HPP_
