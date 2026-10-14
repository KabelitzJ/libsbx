// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <libsbx/async/work_stealing_deque.hpp>
#include <libsbx/async/worker_pool.hpp>

using namespace sbx::async;

TEST(work_stealing_deque_test, pop_is_lifo_and_steal_is_fifo) {
  auto deque = work_stealing_deque<std::uint32_t, 4u>{};

  EXPECT_FALSE(deque.pop().has_value());
  EXPECT_FALSE(deque.steal().has_value());

  EXPECT_TRUE(deque.push(1u));
  EXPECT_TRUE(deque.push(2u));
  EXPECT_TRUE(deque.push(3u));
  EXPECT_TRUE(deque.push(4u));
  EXPECT_FALSE(deque.push(5u)); // full

  EXPECT_EQ(deque.steal(), 1u);
  EXPECT_EQ(deque.pop(), 4u);
  EXPECT_EQ(deque.steal(), 2u);
  EXPECT_EQ(deque.pop(), 3u);

  EXPECT_FALSE(deque.pop().has_value());
  EXPECT_FALSE(deque.steal().has_value());
  EXPECT_TRUE(deque.is_empty());

  // Wraps around the ring after being drained.
  EXPECT_TRUE(deque.push(6u));
  EXPECT_EQ(deque.pop(), 6u);
}

TEST(work_stealing_deque_test, concurrent_owner_and_thieves_take_every_item_exactly_once) {
  static constexpr auto item_count = std::uint32_t{1u} << 20u;
  static constexpr auto thief_count = 3u;

  auto deque = std::make_unique<work_stealing_deque<std::uint32_t, 1024u>>();
  auto taken = std::vector<std::atomic<std::uint32_t>>(item_count);
  auto is_done = std::atomic<bool>{false};

  auto thieves = std::vector<std::thread>{};

  for (auto thief = 0u; thief < thief_count; ++thief) {
    thieves.emplace_back([&]() {
      while (!is_done.load(std::memory_order_acquire)) {
        if (const auto item = deque->steal()) {
          taken[*item].fetch_add(1u, std::memory_order_relaxed);
        }
      }
    });
  }

  for (auto item = 0u; item < item_count; ++item) {
    while (!deque->push(item)) {
      if (const auto popped = deque->pop()) {
        taken[*popped].fetch_add(1u, std::memory_order_relaxed);
      }
    }

    // Pop now and then, so the owner races thieves for the last element too.
    if (item % 7u == 0u) {
      if (const auto popped = deque->pop()) {
        taken[*popped].fetch_add(1u, std::memory_order_relaxed);
      }
    }
  }

  while (const auto popped = deque->pop()) {
    taken[*popped].fetch_add(1u, std::memory_order_relaxed);
  }

  is_done.store(true, std::memory_order_release);

  for (auto& thief : thieves) {
    thief.join();
  }

  for (auto item = 0u; item < item_count; ++item) {
    ASSERT_EQ(taken[item].load(), 1u) << "item " << item;
  }
}

TEST(worker_pool_test, parallel_for_visits_every_index_once) {
  auto pool = worker_pool{4u};

  auto visits = std::vector<std::atomic<std::uint32_t>>(100000u);

  pool.parallel_for(0u, visits.size(), 64u, [&](std::size_t index) {
    visits[index].fetch_add(1u, std::memory_order_relaxed);
  });

  for (const auto& visit : visits) {
    ASSERT_EQ(visit.load(), 1u);
  }
}

TEST(worker_pool_test, nested_parallel_for_completes) {
  auto pool = worker_pool{4u};

  auto total = std::atomic<std::uint32_t>{0u};

  pool.parallel_for(0u, 16u, 1u, [&](std::size_t) {
    pool.parallel_for(0u, 1000u, 10u, [&](std::size_t) {
      total.fetch_add(1u, std::memory_order_relaxed);
    });
  });

  EXPECT_EQ(total.load(), 16000u);
}

TEST(worker_pool_test, wait_rethrows_a_job_exception_and_the_pool_keeps_working) {
  auto pool = worker_pool{2u};

  auto counter = job_counter{};
  auto ran = std::atomic<std::uint32_t>{0u};

  for (auto index = 0u; index < 8u; ++index) {
    pool.schedule(counter, [&ran, index]() {
      ran.fetch_add(1u, std::memory_order_relaxed);

      if (index == 3u) {
        throw std::runtime_error{"job failed"};
      }
    });
  }

  EXPECT_THROW(pool.wait(counter), std::runtime_error);
  EXPECT_EQ(ran.load(), 8u);

  // The counter is reusable and the exception doesn't stick.
  pool.schedule(counter, [&ran]() { ran.fetch_add(1u, std::memory_order_relaxed); });
  EXPECT_NO_THROW(pool.wait(counter));
  EXPECT_EQ(ran.load(), 9u);
}

TEST(worker_pool_test, zero_workers_run_everything_on_the_waiting_thread) {
  auto pool = worker_pool{0u};

  const auto caller = std::this_thread::get_id();
  auto other_thread = std::atomic<bool>{false};

  pool.parallel_for(0u, 1000u, 10u, [&](std::size_t) {
    if (std::this_thread::get_id() != caller) {
      other_thread.store(true, std::memory_order_relaxed);
    }
  });

  auto counter = job_counter{};
  pool.schedule(counter, [&]() {
    if (std::this_thread::get_id() != caller) {
      other_thread.store(true, std::memory_order_relaxed);
    }
  });
  pool.wait(counter);

  EXPECT_FALSE(other_thread.load());
}

TEST(worker_pool_test, jobs_spawned_on_one_worker_are_stolen_by_others) {
  auto pool = worker_pool{4u};

  auto mutex = std::mutex{};
  auto threads = std::set<std::thread::id>{};
  auto sink = std::atomic<std::uint32_t>{0u};
  auto root_on_worker = std::atomic<bool>{false};

  auto counter = job_counter{};

  // The root job spawns all 4096 chunks into its worker's own deque; the other workers only get work by stealing.
  pool.schedule(counter, [&]() {
    root_on_worker.store(pool.is_worker_thread(), std::memory_order_relaxed);

    pool.parallel_for(0u, 65536u, 16u, [&](std::size_t index) {
      auto busy = static_cast<std::uint32_t>(index);

      for (auto step = 0u; step < 2000u; ++step) {
        busy = busy * 1664525u + 1013904223u;
      }

      sink.fetch_add(busy, std::memory_order_relaxed);

      auto lock = std::lock_guard{mutex};
      threads.insert(std::this_thread::get_id());
    });
  });

  // Poll instead of wait(): a waiting thread would run (and steal) jobs itself, and could even take the root job.
  while (!counter.is_done()) {
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }

  pool.wait(counter);

  EXPECT_TRUE(root_on_worker.load());
  EXPECT_GT(threads.size(), 1u);
}
