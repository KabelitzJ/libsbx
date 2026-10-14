// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/async/worker_pool.hpp>

#include <functional>
#include <string>

#include <fmt/format.h>

#include <libsbx/utility/profiler.hpp>

namespace sbx::async {

// Yield rounds an idle worker spins through before it sleeps; keeps wake-up latency low between back-to-back batches.
static constexpr auto idle_spin_rounds = 64u;

worker_pool::worker_pool(std::uint32_t worker_count) {
  _workers.reserve(worker_count);

  for (auto index = std::uint32_t{0u}; index < worker_count; ++index) {
    _workers.push_back(std::make_unique<worker>());
  }

  // Started only once every deque exists, since any worker may steal from any other.
  for (auto index = std::uint32_t{0u}; index < worker_count; ++index) {
    _workers[index]->thread = std::thread{[this, index]() { _worker_loop(index); }};
  }
}

worker_pool::~worker_pool() {
  _is_stopping.store(true, std::memory_order_seq_cst);
  _epoch.fetch_add(1u, std::memory_order_seq_cst);
  _epoch.notify_all();

  for (auto& worker : _workers) {
    worker->thread.join();
  }
}

auto worker_pool::wait(job_counter& counter) -> void {
  SBX_PROFILE_SCOPE("async::wait");

  while (!counter.is_done()) {
    if (auto next = _try_acquire()) {
      next->run();
    } else {
      // ponytail: yield-spin while the last jobs finish elsewhere; park on the counter (atomic wait) if it burns CPU.
      std::this_thread::yield();
    }
  }

  if (auto exception = counter._reset()) {
    std::rethrow_exception(exception);
  }
}

auto worker_pool::is_worker_thread() const noexcept -> bool {
  return _current_worker() != invalid_worker;
}

auto worker_pool::_submit(job& work) -> void {
  const auto pointer = memory::make_observer(work);
  const auto index = _current_worker();

  // Only a deque's owner may push to it; everyone else (and a full deque) goes through the injection queue.
  if (index == invalid_worker || !_workers[index]->deque.push(pointer)) {
    auto lock = std::lock_guard{_injection_mutex};

    _injection.push_back(pointer);
    _injection_size.fetch_add(1u, std::memory_order_relaxed);
  }

  // A worker reads the epoch before its last look for work and sleeps on that value, so this bump either shows it the job or wakes it.
  _epoch.fetch_add(1u, std::memory_order_seq_cst);

  if (_sleepers.load(std::memory_order_seq_cst) > 0u) {
    _epoch.notify_one();
  }
}

auto worker_pool::_try_acquire() -> memory::observer_ptr<job> {
  const auto self = _current_worker();

  if (self != invalid_worker) {
    if (const auto popped = _workers[self]->deque.pop()) {
      return *popped;
    }
  }

  if (_injection_size.load(std::memory_order_relaxed) > 0u) {
    auto lock = std::lock_guard{_injection_mutex};

    if (!_injection.empty()) {
      const auto injected = _injection.front();

      _injection.pop_front();
      _injection_size.fetch_sub(1u, std::memory_order_relaxed);

      return injected;
    }
  }

  const auto count = worker_count();

  if (count == 0u) {
    return nullptr;
  }

  const auto start = _next_steal_random() % count;

  for (auto offset = std::uint32_t{0u}; offset < count; ++offset) {
    const auto victim = (start + offset) % count;

    if (victim == self) {
      continue;
    }

    if (const auto stolen = _workers[victim]->deque.steal()) {
      return *stolen;
    }
  }

  return nullptr;
}

auto worker_pool::_current_worker() const noexcept -> std::uint32_t {
  return _current_pool.get() == this ? _current_worker_index : invalid_worker;
}

auto worker_pool::_worker_loop(std::uint32_t index) -> void {
  _current_pool = memory::observer_ptr<const worker_pool>{this};
  _current_worker_index = index;
  _steal_random_state = static_cast<std::uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())) | 1u;

  const auto name = fmt::format("Worker {}", index);
  SBX_PROFILE_THREAD_NAME(name.c_str());

  auto idle_rounds = 0u;

  while (true) {
    if (auto next = _try_acquire()) {
      next->run();
      idle_rounds = 0u;
      continue;
    }

    if (_is_stopping.load(std::memory_order_acquire)) {
      return;
    }

    if (++idle_rounds < idle_spin_rounds) {
      std::this_thread::yield();
      continue;
    }

    idle_rounds = 0u;

    const auto epoch = _epoch.load(std::memory_order_seq_cst);

    if (auto next = _try_acquire()) {
      next->run();
      continue;
    }

    if (_is_stopping.load(std::memory_order_acquire)) {
      return;
    }

    {
      SBX_PROFILE_SCOPE_COLORED("async::sleep", 0x606060);

      _sleepers.fetch_add(1u, std::memory_order_seq_cst);
      _epoch.wait(epoch, std::memory_order_seq_cst);
      _sleepers.fetch_sub(1u, std::memory_order_seq_cst);
    }
  }
}

auto worker_pool::_next_steal_random() noexcept -> std::uint32_t {
  _steal_random_state ^= _steal_random_state << 13u;
  _steal_random_state ^= _steal_random_state >> 17u;
  _steal_random_state ^= _steal_random_state << 5u;

  return _steal_random_state;
}

auto worker_pool::_wait_ignoring_exceptions(job_counter& counter) noexcept -> void {
  while (!counter.is_done()) {
    if (auto next = _try_acquire()) {
      next->run();
    } else {
      std::this_thread::yield();
    }
  }

  static_cast<void>(counter._reset());
}

} // namespace sbx::async
