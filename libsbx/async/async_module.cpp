// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/async/async_module.hpp>

#include <algorithm>
#include <thread>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

namespace sbx::async {

static auto default_worker_count() -> std::uint32_t {
  const auto& config = core::engine::config();

  if (config.worker_count) {
    return *config.worker_count;
  }

  const auto reserved = config.threading == core::threading_policy::multi_threaded ? 2u : 1u;
  const auto cores = std::max(std::thread::hardware_concurrency(), 1u);

  return cores > reserved ? cores - reserved : 1u;
}

async_module::async_module()
: _pool{default_worker_count()} {
  utility::logger<"async">::info("Started {} worker threads", _pool.worker_count());
}

} // namespace sbx::async
