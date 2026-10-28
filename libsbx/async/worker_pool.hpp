// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASYNC_WORKER_POOL_HPP_
#define LIBSBX_ASYNC_WORKER_POOL_HPP_

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/async/job.hpp>
#include <libsbx/async/work_stealing_deque.hpp>

namespace sbx::async {

/**
 * @brief A fixed set of worker threads that run jobs, with a work-stealing deque per worker.
 *
 * Any thread may submit and wait. Workers push to their own deque; other threads push to a shared injection queue.
 * A thread waiting on a counter runs other jobs (its own, injected, or stolen) instead of blocking, so nested waits can't deadlock.
 *
 * Jobs must not make structural ECS changes, call into managed (C#) code, record Vulkan commands or use ImGui.
 * Write results to per-index or per-chunk slots and merge them after the wait.
 */
class worker_pool : public utility::noncopyable {

public:

  /** @brief 0 workers is valid: everything then runs on the waiting thread. */
  explicit worker_pool(std::uint32_t worker_count);

  ~worker_pool();

  template<typename Callable>
  auto schedule(job_counter& counter, Callable&& callable) -> void {
    _submit(counter._emplace(std::forward<Callable>(callable)));
  }

  /** @brief Runs jobs until every job of @p counter is done, then rethrows the first exception one of them threw. */
  auto wait(job_counter& counter) -> void;

  /**
   * @brief Calls @p function(chunk_begin, chunk_end) for consecutive chunks of at most @p grain indices covering [begin, end), and waits for all of them.
   *
   * The calling thread runs the first chunk itself. Ranges no larger than one chunk, or a pool without workers, run inline.
   */
  template<typename Function>
  requires (std::is_invocable_v<Function&, std::size_t, std::size_t>)
  auto parallel_for_chunks(std::size_t begin, std::size_t end, std::size_t grain, Function&& function) -> void {
    if (end <= begin) {
      return;
    }

    grain = std::max(grain, std::size_t{1u});

    if (end - begin <= grain || _workers.empty()) {
      std::invoke(function, begin, end);
      return;
    }

    auto counter = job_counter{};

    for (auto chunk_begin = begin + grain; chunk_begin < end; chunk_begin += grain) {
      const auto chunk_end = std::min(chunk_begin + grain, end);

      schedule(counter, [&function, chunk_begin, chunk_end]() { std::invoke(function, chunk_begin, chunk_end); });
    }

    try {
      std::invoke(function, begin, begin + grain);
    } catch (...) {
      // The scheduled chunks still reference function and counter on this stack frame.
      _wait_ignoring_exceptions(counter);
      throw;
    }

    wait(counter);
  }

  /** @brief parallel_for_chunks() calling @p function(index) for every index. */
  template<typename Function>
  requires (std::is_invocable_v<Function&, std::size_t>)
  auto parallel_for(std::size_t begin, std::size_t end, std::size_t grain, Function&& function) -> void {
    parallel_for_chunks(begin, end, grain, [&function](std::size_t chunk_begin, std::size_t chunk_end) {
      for (auto index = chunk_begin; index < chunk_end; ++index) {
        std::invoke(function, index);
      }
    });
  }

  [[nodiscard]] auto worker_count() const noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(_workers.size());
  }

  /** @brief Whether the calling thread is one of this pool's workers. */
  [[nodiscard]] auto is_worker_thread() const noexcept -> bool;

private:

  inline static constexpr auto invalid_worker = std::uint32_t{0xFFFFFFFFu};

  using deque_type = work_stealing_deque<memory::observer_ptr<job>>;

  struct worker {
    deque_type deque{};
    std::thread thread{};
  }; // struct worker

  auto _submit(job& work) -> void;

  [[nodiscard]] auto _try_acquire() -> memory::observer_ptr<job>;

  [[nodiscard]] auto _current_worker() const noexcept -> std::uint32_t;

  auto _worker_loop(std::uint32_t index) -> void;

  auto _wait_ignoring_exceptions(job_counter& counter) noexcept -> void;

  // Runs other jobs while @p counter has pending ones, parking on _epoch when there's nothing to help with.
  auto _help_until_done(job_counter& counter) -> void;

  // Runs @p work and, if it finished its batch, wakes parked waiters.
  auto _run(job& work) -> void;

  // xorshift32 over _steal_random_state, seeded per thread on first use.
  [[nodiscard]] static auto _next_steal_random() noexcept -> std::uint32_t;

  // Which pool and deque the calling thread works for; the pool pointer keeps workers of different pools apart.
  inline static thread_local auto _current_pool = memory::observer_ptr<const worker_pool>{};
  inline static thread_local auto _current_worker_index = std::uint32_t{0u};
  inline static thread_local auto _steal_random_state = std::uint32_t{0u};

  // unique_ptr: each deque is ~32 KiB of cache-line-aligned atomics and must never move.
  std::vector<std::unique_ptr<worker>> _workers{};

  std::mutex _injection_mutex{};
  std::deque<memory::observer_ptr<job>> _injection{};
  std::atomic<std::uint32_t> _injection_size{0u};

  // Bumped on every submit and every finished batch; idle workers and parked waiters sleep on it (C++20 atomic wait), and only get a notify when one is asleep.
  std::atomic<std::uint32_t> _epoch{0u};
  std::atomic<std::uint32_t> _sleepers{0u};
  std::atomic<bool> _is_stopping{false};

}; // class worker_pool

} // namespace sbx::async

#endif // LIBSBX_ASYNC_WORKER_POOL_HPP_
