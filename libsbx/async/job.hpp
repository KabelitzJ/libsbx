// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_ASYNC_JOB_HPP_
#define LIBSBX_ASYNC_JOB_HPP_

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/function_ptr.hpp>
#include <libsbx/utility/profiler.hpp>

#include <libsbx/memory/observer_ptr.hpp>

namespace sbx::async {

/** @brief The completion state a batch of jobs shares: how many are still pending and the first exception any of them threw. */
struct job_sync {
  std::atomic<std::uint32_t> pending{0u};
  std::atomic<bool> has_exception{false};
  std::exception_ptr exception{};
}; // struct job_sync

/**
 * @brief One unit of work: a callable stored inline (no allocation) plus the batch it belongs to.
 *
 * Never moved once constructed, since worker deques hold its address.
 */
class job : public utility::noncopyable {

  using storage_function_ptr = utility::function_ptr<void, memory::observer_ptr<std::byte>>;

public:

  inline static constexpr auto storage_size = std::size_t{64u};

  template<typename Callable>
  requires (std::is_invocable_r_v<void, std::decay_t<Callable>&>)
  job(Callable&& callable, job_sync& sync)
  : _sync{memory::make_observer(sync)} {
    using stored_type = std::decay_t<Callable>;

    static_assert(sizeof(stored_type) <= storage_size, "Job captures exceed the inline storage; capture a pointer to the state instead.");
    static_assert(alignof(stored_type) <= alignof(std::max_align_t), "Job captures are over-aligned.");

    std::construct_at(reinterpret_cast<stored_type*>(_storage.data()), std::forward<Callable>(callable));

    _invoke = [](memory::observer_ptr<std::byte> storage) { std::invoke(*std::launder(reinterpret_cast<stored_type*>(storage.get()))); };
    _destroy = [](memory::observer_ptr<std::byte> storage) { std::destroy_at(std::launder(reinterpret_cast<stored_type*>(storage.get()))); };
  }

  job(job&&) = delete;

  ~job() {
    std::invoke(_destroy, memory::make_observer(_storage.data()));
  }

  /** @brief Runs the callable and marks it done. The job must not be touched afterwards: its owner may destroy it right away. */
  auto run() -> void {
    SBX_PROFILE_SCOPE("async::job");

    try {
      std::invoke(_invoke, memory::make_observer(_storage.data()));
    } catch (...) {
      if (!_sync->has_exception.exchange(true, std::memory_order_relaxed)) {
        _sync->exception = std::current_exception();
      }
    }

    _sync->pending.fetch_sub(1u, std::memory_order_release);
  }

private:

  alignas(std::max_align_t) std::array<std::byte, storage_size> _storage{};
  storage_function_ptr _invoke{};
  storage_function_ptr _destroy{};
  memory::observer_ptr<job_sync> _sync{};

}; // class job

/**
 * @brief A batch of jobs to wait on. Owns the jobs it was given, so it has to outlive them: keep it alive until worker_pool::wait() on it has returned.
 *
 * Reusable after a wait.
 */
class job_counter : public utility::noncopyable {

  friend class worker_pool;

public:

  job_counter() = default;

  job_counter(job_counter&&) = delete;

  ~job_counter() {
    utility::assert_that(is_done(), "A job_counter was destroyed with jobs still pending; wait on it first.");
  }

  [[nodiscard]] auto is_done() const noexcept -> bool {
    return _sync.pending.load(std::memory_order_acquire) == 0u;
  }

private:

  template<typename Callable>
  auto _emplace(Callable&& callable) -> job& {
    _sync.pending.fetch_add(1u, std::memory_order_relaxed);

    return _jobs.emplace_back(std::forward<Callable>(callable), _sync);
  }

  // Only after every job is done; returns the first exception and resets for reuse.
  auto _reset() -> std::exception_ptr {
    _jobs.clear();

    auto exception = std::exchange(_sync.exception, nullptr);
    _sync.has_exception.store(false, std::memory_order_relaxed);

    return exception;
  }

  job_sync _sync{};

  // Stable addresses while workers hold them.
  // ponytail: std::deque allocates a block per few jobs; swap for a block arena if Tracy shows allocator time.
  std::deque<job> _jobs{};

}; // class job_counter

} // namespace sbx::async

#endif // LIBSBX_ASYNC_JOB_HPP_
